#include "DX12Context.h"
#include "Scene.h"
#include "Camera.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    uint32_t FindHighestSupportedSampleCount(ID3D12Device *device, DXGI_FORMAT format, uint32_t maxRequestedSampleCount)
    {
        if (!device)
            return 1;

        const uint32_t candidates[] = {16, 8, 4, 2};
        for (uint32_t sampleCount : candidates)
        {
            if (sampleCount > maxRequestedSampleCount)
                continue;

            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaFeature = {};
            msaaFeature.Format = format;
            msaaFeature.SampleCount = sampleCount;
            msaaFeature.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

            if (SUCCEEDED(device->CheckFeatureSupport(
                    D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                    &msaaFeature,
                    sizeof(msaaFeature))) &&
                msaaFeature.NumQualityLevels > 0)
            {
                return sampleCount;
            }
        }

        return 1;
    }

    uint32_t FindHighestCommonSampleCount(
        ID3D12Device *device,
        DXGI_FORMAT colorFormat,
        DXGI_FORMAT depthFormat,
        uint32_t maxRequestedSampleCount)
    {
        if (!device)
            return 1;

        const uint32_t candidates[] = {16, 8, 4, 2};
        for (uint32_t sampleCount : candidates)
        {
            if (sampleCount > maxRequestedSampleCount)
                continue;

            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS colorFeature = {};
            colorFeature.Format = colorFormat;
            colorFeature.SampleCount = sampleCount;
            colorFeature.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS depthFeature = {};
            depthFeature.Format = depthFormat;
            depthFeature.SampleCount = sampleCount;
            depthFeature.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;

            const bool colorSupported = SUCCEEDED(device->CheckFeatureSupport(
                                            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                            &colorFeature,
                                            sizeof(colorFeature))) &&
                                        colorFeature.NumQualityLevels > 0;

            const bool depthSupported = SUCCEEDED(device->CheckFeatureSupport(
                                            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                            &depthFeature,
                                            sizeof(depthFeature))) &&
                                        depthFeature.NumQualityLevels > 0;

            if (colorSupported && depthSupported)
                return sampleCount;
        }

        return 1;
    }

    void LogMsaaSupport(
        ID3D12Device *device,
        DXGI_FORMAT colorFormat,
        DXGI_FORMAT depthFormat,
        uint32_t requestedSampleCount,
        uint32_t selectedSampleCount)
    {
        if (!device)
            return;

        const uint32_t maxColorSamples = FindHighestSupportedSampleCount(device, colorFormat, 16);
        const uint32_t maxDepthSamples = FindHighestSupportedSampleCount(device, depthFormat, 16);
        const uint32_t maxCommonSamples = FindHighestCommonSampleCount(device, colorFormat, depthFormat, 16);

        char buffer[256] = {};
        sprintf_s(
            buffer,
            "DX12Context::Init MSAA support - color max: %u, depth max: %u, common max: %u, requested: %u, selected: %u\n",
            maxColorSamples,
            maxDepthSamples,
            maxCommonSamples,
            requestedSampleCount,
            selectedSampleCount);
        OutputDebugStringA(buffer);
    }

    // Frustum plane: a*x + b*y + c*z + d > 0 means the point is on the inside.
    struct FrustumPlane { float a, b, c, d; };

    // Extract 6 planes from a combined view-proj matrix (row-major, row-vector convention).
    // clip = worldPos_row * VP; inside iff -w<x<w, -w<y<w, 0<z<w (DX12 NDC).
    void ExtractFrustumPlanes(const mat4& vp, FrustumPlane planes[6])
    {
        const float* m = vp.m; // m[row*4 + col]
        // clip_x = px*m[0] + py*m[4] + pz*m[8]  + m[12]
        // clip_y = px*m[1] + py*m[5] + pz*m[9]  + m[13]
        // clip_z = px*m[2] + py*m[6] + pz*m[10] + m[14]
        // clip_w = px*m[3] + py*m[7] + pz*m[11] + m[15]
        planes[0] = { m[0]+m[3],  m[4]+m[7],  m[8]+m[11],  m[12]+m[15] }; // left
        planes[1] = { m[3]-m[0],  m[7]-m[4],  m[11]-m[8],  m[15]-m[12] }; // right
        planes[2] = { m[1]+m[3],  m[5]+m[7],  m[9]+m[11],  m[13]+m[15] }; // bottom
        planes[3] = { m[3]-m[1],  m[7]-m[5],  m[11]-m[9],  m[15]-m[13] }; // top
        planes[4] = { m[2],        m[6],        m[10],        m[14]       }; // near (DX z > 0)
        planes[5] = { m[3]-m[2],  m[7]-m[6],  m[11]-m[10], m[15]-m[14] }; // far
    }

    bool PointInFrustum(const FrustumPlane planes[6], float px, float py, float pz)
    {
        for (int i = 0; i < 6; ++i)
            if (planes[i].a * px + planes[i].b * py + planes[i].c * pz + planes[i].d <= 0.0f)
                return false;
        return true;
    }

} // anonymous namespace

DX12Context::~DX12Context()
{
    Shutdown();
}

bool DX12Context::Init(HWND hwnd, uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
    const uint32_t requestedMsaaSampleCount = m_msaaSampleCount;

    if (!CreateDevice())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateDevice\n");
        return false;
    }

    m_msaaSampleCount = FindHighestCommonSampleCount(
        m_device.Get(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        m_depthStencilFormat,
        requestedMsaaSampleCount);

    LogMsaaSupport(
        m_device.Get(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        m_depthStencilFormat,
        requestedMsaaSampleCount,
        m_msaaSampleCount);

    if (m_msaaSampleCount != requestedMsaaSampleCount)
    {
        OutputDebugStringA("DX12Context::Init requested MSAA level unavailable, using fallback sample count\n");
    }

    if (!CreateCommandObjects())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateCommandObjects\n");
        return false;
    }

    if (!CreateSwapChain(hwnd, width, height))
    {
        OutputDebugStringA("DX12Context::Init failed in CreateSwapChain\n");
        return false;
    }

    if (!CreateRenderTargetViews())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateRenderTargetViews\n");
        return false;
    }

    if (!CreateDSVHeap())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateDSVHeap\n");
        return false;
    }

    if (!CreateDepthStencilBuffer())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateDepthStencilBuffer\n");
        return false;
    }

    if (!CreateFenceAndEvent())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateFenceAndEvent\n");
        return false;
    }

    if (!CreateShadowMapResources())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateShadowMapResources\n");
        return false;
    }

    if (!CreateShadowPipeline())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateShadowPipeline\n");
        return false;
    }

    if (!CreateShadowSpritePipeline())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateShadowSpritePipeline\n");
        return false;
    }

    if (!CreateInstancedSpritePipeline())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateInstancedSpritePipeline\n");
        return false;
    }

    if (!CreateParticlePipeline())
    {
        OutputDebugStringA("DX12Context::Init failed in CreateParticlePipeline\n");
        return false;
    }

    if (!CreatePostProcessResources())
    {
        OutputDebugStringA("DX12Context::Init failed in CreatePostProcessResources\n");
        return false;
    }

    if (!CreatePostProcessPipelines())
    {
        OutputDebugStringA("DX12Context::Init failed in CreatePostProcessPipelines\n");
        return false;
    }

    // Per-frame constant buffer: view + proj + lightViewProj (48 floats, 256-byte aligned per frame)
    {
        D3D12_HEAP_PROPERTIES uploadHeap = {};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC cbDesc = {};
        cbDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        cbDesc.Width            = 256 * FrameCount;
        cbDesc.Height           = 1;
        cbDesc.DepthOrArraySize = 1;
        cbDesc.MipLevels        = 1;
        cbDesc.Format           = DXGI_FORMAT_UNKNOWN;
        cbDesc.SampleDesc.Count = 1;
        cbDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(m_device->CreateCommittedResource(
                &uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&m_perFrameCb))))
        {
            OutputDebugStringA("DX12Context::Init failed to create per-frame constant buffer\n");
            return false;
        }
        m_perFrameCb->Map(0, nullptr, reinterpret_cast<void**>(&m_perFrameCbMapped));
    }

    // Build light view-proj from the shadow direction used for rendering
    {
        const vec3 lightDir    = Vec3Normalize(vec3(-0.40f, -0.95f, -0.55f));
        const vec3 sceneCenter(0.0f, 0.0f, -5.0f);
        const vec3 lightPos    = sceneCenter - (lightDir * 20.0f);
        m_lightPos             = lightPos;
        const mat4 lightView   = MatrixLookAtRH(lightPos, sceneCenter, vec3(0.0f, 1.0f, 0.0f));
        const mat4 lightProj   = MatrixOrthographicRH(22.0f, 22.0f, 0.1f, 45.0f);
        m_lightViewProj        = MatrixMultiply(lightView, lightProj);
    }

    OutputDebugStringA("DX12Context::Init succeeded\n");
    return true;
}

void DX12Context::Shutdown()
{
    if (m_device)
    {
        WaitForGpu();
    }

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        m_renderTargets[i].Reset();
        m_commandAllocators[i].Reset();
    }

    m_commandList.Reset();
    m_rtvHeap.Reset();
    m_dsvHeap.Reset();
    m_msaaColorTarget.Reset();
    m_depthStencil.Reset();
    m_swapChain.Reset();
    m_commandQueue.Reset();
    m_fence.Reset();
    m_device.Reset();
    m_dxgiFactory.Reset();
}

void DX12Context::LogAdapterInfo(Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter)
{
    if (!adapter)
        return;

    DXGI_ADAPTER_DESC1 desc = {};
    adapter->GetDesc1(&desc);

    wchar_t buf[512];
    swprintf_s(buf,
               L"[DX12] ================================\n"
               L"[DX12] Adapter:       %s\n"
               L"[DX12] Vendor ID:     0x%04X\n"
               L"[DX12] Device ID:     0x%04X\n"
               L"[DX12] VRAM:          %zu MB\n"
               L"[DX12] Shared RAM:    %zu MB\n"
               L"[DX12] System RAM:    %zu MB\n"
               L"[DX12] ================================\n",
               desc.Description,
               desc.VendorId,
               desc.DeviceId,
               desc.DedicatedVideoMemory / (1024 * 1024),
               desc.SharedSystemMemory / (1024 * 1024),
               desc.DedicatedSystemMemory / (1024 * 1024));

    OutputDebugStringW(buf);
}

void DX12Context::LogAllAdapters()
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         m_dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i)
    {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);

        wchar_t buf[256];
        swprintf_s(buf,
                   L"[DX12] Adapter %u: %s | VRAM: %zu MB | Software: %s\n",
                   i,
                   desc.Description,
                   desc.DedicatedVideoMemory / (1024 * 1024),
                   (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? L"YES" : L"NO");

        OutputDebugStringW(buf);
    }
}

bool DX12Context::CreateDevice()
{
    // 1. Create factory first
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    HRESULT hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_dxgiFactory));
    if (FAILED(hr))
    {
        OutputDebugStringW(L"[DX12] Failed to create DXGI factory\n");
        return false;
    }

    // 2. Log all adapters so you can see what's available
    LogAllAdapters();

    // 3. Pick the best hardware adapter explicitly
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter1;
    for (UINT i = 0;
         m_dxgiFactory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND;
         ++i)
    {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter1->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Test DX12 support without creating a device
        hr = D3D12CreateDevice(adapter1.Get(),
                               D3D_FEATURE_LEVEL_11_0,
                               __uuidof(ID3D12Device),
                               nullptr); // nullptr = test only, no device created
        if (SUCCEEDED(hr))
        {
            adapter1.As(&m_adapter);
            break;
        }
    }

    if (!m_adapter)
    {
        OutputDebugStringW(L"[DX12] No hardware adapter found, falling back to WARP\n");
        Microsoft::WRL::ComPtr<IDXGIAdapter> warp;
        m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
        warp.As(&m_adapter);
    }

    // 4. Create device with explicit adapter
    hr = D3D12CreateDevice(m_adapter.Get(),
                           D3D_FEATURE_LEVEL_11_0,
                           IID_PPV_ARGS(&m_device));
    if (FAILED(hr))
    {
        wchar_t buf[128];
        swprintf_s(buf, L"[DX12] CreateDevice failed. HRESULT: 0x%08X\n", hr);
        OutputDebugStringW(buf);
        return false;
    }

    LogAdapterInfo(m_adapter);
    return true;
}

bool DX12Context::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue))))
        return false;

    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i]))))
            return false;
    }

    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList))))
        return false;

    if (FAILED(m_commandList->Close()))
        return false;

    return true;
}

bool DX12Context::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height)
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1)))
        return false;

    if (FAILED(swapChain1.As(&m_swapChain)))
        return false;

    if (FAILED(m_dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER)))
        return false;

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    return true;
}

bool DX12Context::CreateRenderTargetViews()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount + 1;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]))))
            return false;

        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    D3D12_RESOURCE_DESC msaaDesc = {};
    msaaDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    msaaDesc.Alignment = 0;
    msaaDesc.Width = m_width;
    msaaDesc.Height = m_height;
    msaaDesc.DepthOrArraySize = 1;
    msaaDesc.MipLevels = 1;
    msaaDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    msaaDesc.SampleDesc.Count = m_msaaSampleCount;
    msaaDesc.SampleDesc.Quality = 0;
    msaaDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    msaaDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &msaaDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            nullptr,
            IID_PPV_ARGS(&m_msaaColorTarget))))
    {
        return false;
    }

    m_msaaColorTarget->SetName(L"MSAA Color Target");

    m_device->CreateRenderTargetView(m_msaaColorTarget.Get(), nullptr, rtvHandle);

    return true;
}

bool DX12Context::CreateDSVHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // ← must be NONE for DSV
    desc.NodeMask = 0;

    HRESULT hr = m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvHeap));
    if (FAILED(hr))
    {
        wchar_t buf[64];
        swprintf_s(buf, L"[DX12] CreateDescriptorHeap DSV failed: 0x%08X\n", hr);
        OutputDebugStringW(buf);
        return false;
    }

    // Verify heap handle is valid
    auto handle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    wchar_t buf[128];
    swprintf_s(buf, L"[DX12] DSV heap handle: %zu  type=%u  count=%u  flags=%u\n",
               handle.ptr, desc.Type, desc.NumDescriptors, desc.Flags);
    OutputDebugStringW(buf);

    return true;
}

bool DX12Context::CreateDepthStencilBuffer()
{
    OutputDebugStringW(L"[DX12] DSV: starting\n");

    if (!m_device)
    {
        OutputDebugStringW(L"[DX12] DSV: device is null\n");
        return false;
    }
    if (!m_dsvHeap)
    {
        OutputDebugStringW(L"[DX12] DSV: dsvHeap is null\n");
        return false;
    }
    if (m_width == 0 || m_height == 0)
    {
        OutputDebugStringW(L"[DX12] DSV: zero dimension\n");
        return false;
    }

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = m_width;
    desc.Height = m_height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.SampleDesc.Count = m_msaaSampleCount;
    desc.SampleDesc.Quality = m_msaaQuality;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    wchar_t buf[256];
    swprintf_s(buf,
               L"[DX12] DSV desc: w=%u h=%u fmt=%u "
               L"sampleCount=%u sampleQuality=%u\n",
               (UINT)desc.Width, desc.Height, desc.Format,
               desc.SampleDesc.Count, desc.SampleDesc.Quality);
    OutputDebugStringW(buf);

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_depthStencil));

    if (FAILED(hr))
    {
        swprintf_s(buf, L"[DX12] CreateCommittedResource DSV failed: 0x%08X\n", hr);
        OutputDebugStringW(buf);
        return false;
    }

    OutputDebugStringW(L"[DX12] DSV: resource created OK, creating view\n");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    if (m_msaaSampleCount > 1)
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    else
    {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
    }

    swprintf_s(buf,
               L"[DX12] DSV view: fmt=%u dim=%u\n",
               dsvDesc.Format, dsvDesc.ViewDimension);
    OutputDebugStringW(buf);

    m_device->CreateDepthStencilView(
        m_depthStencil.Get(),
        &dsvDesc,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    OutputDebugStringW(L"[DX12] DSV: view created OK\n");

    return true;
}

bool DX12Context::CreateFenceAndEvent()
{
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        HRESULT reason = m_device->GetDeviceRemovedReason();
        wchar_t buf[128];
        swprintf_s(buf, L"[DX12] Device removed. Reason: 0x%08X\n", reason);
        OutputDebugStringW(buf);
        return false;
    }
    else if (FAILED(hr))
    {
        wchar_t buf[128];
        swprintf_s(buf, L"[DX12] CreateFence failed. HRESULT: 0x%08X\n", hr);
        OutputDebugStringW(buf);
        return false;
    }

    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        m_fenceValues[i] = 1;
    }

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
        return false;

    return true;
}

bool DX12Context::CreateShadowMapResources()
{
    // Depth texture (R32_TYPELESS so we can alias as D32_FLOAT DSV and R32_FLOAT SRV)
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width              = kShadowMapSize;
    desc.Height             = kShadowMapSize;
    desc.DepthOrArraySize   = 1;
    desc.MipLevels          = 1;
    desc.Format             = DXGI_FORMAT_R32_TYPELESS;
    desc.SampleDesc.Count   = 1;
    desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format            = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    if (FAILED(m_device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
            IID_PPV_ARGS(&m_shadowMap))))
    {
        return false;
    }
    m_shadowMap->SetName(L"Shadow Map");

    // DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_shadowDsvHeap))))
        return false;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_device->CreateDepthStencilView(
        m_shadowMap.Get(), &dsvDesc,
        m_shadowDsvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

bool DX12Context::CreateShadowPipeline()
{
    // Minimal VS: transform position by world × lightViewProj
    static const char* kShadowVS = R"(
cbuffer ShadowCB : register(b0)
{
    row_major float4x4 worldMatrix;
    row_major float4x4 lightViewProjMatrix;
};
float4 VSMain(float3 position : POSITION) : SV_POSITION
{
    float4 worldPos = mul(float4(position, 1.0f), worldMatrix);
    return mul(worldPos, lightViewProjMatrix);
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    if (FAILED(D3DCompile(kShadowVS, strlen(kShadowVS), nullptr, nullptr, nullptr,
                          "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    // Root signature: 32 float constants (world 16 + lightVP 16)
    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParam.Constants.Num32BitValues = 32;
    rootParam.Constants.ShaderRegister = 0;
    rootParam.ShaderVisibility         = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters   = &rootParam;
    rootSigDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> serializedSig;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &serializedSig, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, serializedSig->GetBufferPointer(),
                                              serializedSig->GetBufferSize(),
                                              IID_PPV_ARGS(&m_shadowRootSig))))
        return false;

    static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode              = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode              = D3D12_CULL_MODE_BACK;
    rastDesc.FrontCounterClockwise = TRUE;
    rastDesc.DepthBias             = 100;
    rastDesc.SlopeScaledDepthBias  = 2.0f;
    rastDesc.DepthClipEnable       = TRUE;

    D3D12_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = TRUE;
    dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_shadowRootSig.Get();
    psoDesc.VS                    = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState       = rastDesc;
    psoDesc.DepthStencilState     = dsDesc;
    psoDesc.InputLayout           = { inputLayout, 1 };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 0;
    psoDesc.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count      = 1;

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_shadowPso))))
        return false;

    return true;
}

bool DX12Context::CreateShadowSpritePipeline()
{
    // VS passes position + remapped UV to the PS; PS clips chroma-keyed transparent pixels
    static const char* kShadowSpriteShader = R"(
cbuffer ShadowSpriteCB : register(b0)
{
    row_major float4x4 worldMatrix;
    row_major float4x4 lightViewProjMatrix;
    float4 spriteUVRect;
};
Texture2D    spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

struct VSInput  { float3 position : POSITION; float2 uv : TEXCOORD0; };
struct PSInput  { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

PSInput VSMain(VSInput input)
{
    PSInput o;
    float4 wp = mul(float4(input.position, 1.0f), worldMatrix);
    o.position = mul(wp, lightViewProjMatrix);
    o.uv = lerp(spriteUVRect.xy, spriteUVRect.zw, input.uv);
    return o;
}

void PSMain(PSInput input)
{
    float4 texel     = spriteTexture.Sample(spriteSampler, input.uv);
    float3 chromaKey = float3(34.0f / 255.0f, 177.0f / 255.0f, 76.0f / 255.0f);
    float  alpha     = texel.a * smoothstep(0.10f, 0.16f, distance(texel.rgb, chromaKey));
    clip(alpha - 0.05f);
}
)";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    if (FAILED(D3DCompile(kShadowSpriteShader, strlen(kShadowSpriteShader), nullptr, nullptr, nullptr,
                          "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    errorBlob.Reset();
    if (FAILED(D3DCompile(kShadowSpriteShader, strlen(kShadowSpriteShader), nullptr, nullptr, nullptr,
                          "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    // Slot 0: 36 root constants (world 16 + lightVP 16 + uvRect 4), vertex+pixel visible
    // Slot 1: descriptor table — 1 SRV (t0), pixel visible
    D3D12_ROOT_PARAMETER rootParams[2] = {};
    rootParams[0].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants.Num32BitValues = 36;
    rootParams[0].Constants.ShaderRegister = 0;
    rootParams[0].ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors     = 1;
    srvRange.BaseShaderRegister = 0;

    rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
    rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter           = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD           = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister   = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters     = 2;
    rootSigDesc.pParameters       = rootParams;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers   = &staticSampler;
    rootSigDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> serializedSig;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &serializedSig, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, serializedSig->GetBufferPointer(),
                                              serializedSig->GetBufferSize(),
                                              IID_PPV_ARGS(&m_shadowSpriteRootSig))))
        return false;

    // Input layout: POSITION at offset 0, TEXCOORD0 at offset 40 (past normal+color in Vertex)
    static const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode              = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode              = D3D12_CULL_MODE_NONE;  // sprites render both faces
    rastDesc.DepthBias             = 100;
    rastDesc.SlopeScaledDepthBias  = 2.0f;
    rastDesc.DepthClipEnable       = TRUE;

    D3D12_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = TRUE;
    dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_shadowSpriteRootSig.Get();
    psoDesc.VS                    = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS                    = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState       = rastDesc;
    psoDesc.DepthStencilState     = dsDesc;
    psoDesc.InputLayout           = { inputLayout, 2 };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 0;
    psoDesc.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count      = 1;

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_shadowSpritePso))))
        return false;

    return true;
}

bool DX12Context::CreateInstancedSpritePipeline()
{
    // VS reads per-instance world matrix + uvRect from a StructuredBuffer via SV_InstanceID.
    // PS applies chroma-key clip and fog. No vertex buffer — geometry is procedural (SV_VertexID).
    static const char* kShader = R"(
cbuffer PerFrame : register(b0)
{
    row_major float4x4 viewMatrix;
    row_major float4x4 projMatrix;
    row_major float4x4 lightViewProjMatrix;
    float3 cameraEyeWS;
    float  _pad;
};

struct SpriteInstance
{
    row_major float4x4 world;
    float4             uvRect;
    float4             tint;   // rgb = per-sprite colour tint
};
StructuredBuffer<SpriteInstance> g_instances : register(t0);

Texture2D              g_sprite    : register(t1);
Texture2D              g_shadowMap : register(t2);
SamplerState           g_sampler   : register(s0);
SamplerComparisonState g_shadowSmp : register(s1);

struct VSOut
{
    float4 pos      : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float3 viewV    : TEXCOORD1;
    float2 clipYW   : TEXCOORD2;  // for sky-gradient fog
    float  localV   : TEXCOORD3;  // raw quad V (0=bottom, 1=top) for base AO
    float3 worldPos : TEXCOORD4;  // world position for shadow projection
    float3 tintRGB  : TEXCOORD5;  // per-instance colour tint
};

static const float3 kPos[6] = {
    float3(-0.5f, -0.5f, 0.0f), float3( 0.5f, -0.5f, 0.0f), float3( 0.5f,  0.5f, 0.0f),
    float3(-0.5f, -0.5f, 0.0f), float3( 0.5f,  0.5f, 0.0f), float3(-0.5f,  0.5f, 0.0f)
};
static const float2 kUV[6] = {
    float2(0.0f, 1.0f), float2(1.0f, 1.0f), float2(1.0f, 0.0f),
    float2(0.0f, 1.0f), float2(1.0f, 0.0f), float2(0.0f, 0.0f)
};

VSOut VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    SpriteInstance inst = g_instances[iid];
    float4 wp  = mul(float4(kPos[vid], 1.0f), inst.world);
    float4 vp  = mul(wp, viewMatrix);
    VSOut o;
    o.pos      = mul(vp, projMatrix);
    o.uv       = lerp(inst.uvRect.xy, inst.uvRect.zw, kUV[vid]);
    o.viewV    = vp.xyz;
    o.clipYW   = float2(o.pos.y, o.pos.w);
    o.localV   = kUV[vid].y;
    o.worldPos = wp.xyz;
    o.tintRGB  = inst.tint.rgb;
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET
{
    float4 texel  = g_sprite.Sample(g_sampler, i.uv);
    float3 chroma = float3(34.0f / 255.0f, 177.0f / 255.0f, 76.0f / 255.0f);
    float  alpha  = texel.a * smoothstep(0.10f, 0.16f, distance(texel.rgb, chroma));
    clip(alpha - 0.05f);

    float3 rgb = texel.rgb * i.tintRGB;

    // Base AO: darken sprite feet — grounds the sprite visually
    rgb *= lerp(0.55f, 1.0f, i.localV);

    // Shadow map receive — only sample when sprite is inside the light frustum
    float4 lsPos    = mul(float4(i.worldPos, 1.0f), lightViewProjMatrix);
    lsPos.xyz      /= lsPos.w;
    float2 shadowUV = lsPos.xy * float2(0.5f, -0.5f) + 0.5f;
    float  shadow   = 1.0f;  // default: fully lit (outside light frustum = no shadow)
    if (lsPos.x >= -1.0f && lsPos.x <= 1.0f &&
        lsPos.y >= -1.0f && lsPos.y <= 1.0f &&
        lsPos.z >=  0.0f && lsPos.z <= 1.0f)
        shadow = g_shadowMap.SampleCmpLevelZero(g_shadowSmp, shadowUV, lsPos.z - 0.001f);
    rgb *= lerp(0.35f, 1.0f, shadow);

    // Distance fog with sky-gradient colour (matches Material.hlsl)
    float  camDist   = length(i.viewV);
    float  fogFactor = saturate((camDist - 8.0f) / (22.0f - 8.0f));
    float  ndcY      = i.clipYW.x / max(abs(i.clipYW.y), 1e-4f);
    float  skyT      = saturate(ndcY * 0.5f + 0.5f);
    float3 fogColor  = lerp(float3(0.64f, 0.70f, 0.78f), float3(0.24f, 0.38f, 0.62f), skyT);
    rgb = lerp(rgb, fogColor, fogFactor * 0.35f);

    return float4(saturate(rgb), alpha);
}
)";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    if (FAILED(D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
                          "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    errorBlob.Reset();
    if (FAILED(D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
                          "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    // Root sig:
    //   Param 0: root CBV  b0 (ALL)    — perFrame constants
    //   Param 1: root SRV  t0 (VERTEX) — instance StructuredBuffer (address set per-frame)
    //   Param 2: desc table t1 (PIXEL) — sprite atlas (1 SRV)
    D3D12_ROOT_PARAMETER rootParams[3] = {};

    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;  // b0
    rootParams[0].Descriptor.RegisterSpace  = 0;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParams[1].Descriptor.ShaderRegister = 0;  // t0
    rootParams[1].Descriptor.RegisterSpace  = 0;
    rootParams[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

    // Covers t1=sprite (heap slot 0) and t2=shadowMap (heap slot 1) from the material heap
    D3D12_DESCRIPTOR_RANGE spriteRange = {};
    spriteRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    spriteRange.NumDescriptors     = 2;
    spriteRange.BaseShaderRegister = 1;  // t1, t2
    spriteRange.RegisterSpace      = 0;
    spriteRange.OffsetInDescriptorsFromTableStart = 0;

    rootParams[2].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges   = &spriteRange;
    rootParams[2].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};

    // s0 — sprite atlas: point mag (crisp up close), linear min+mip (smooth at distance)
    staticSamplers[0].Filter           = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
    staticSamplers[0].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD           = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister   = 0;  // s0
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1 — shadow comparison sampler
    staticSamplers[1].Filter           = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    staticSamplers[1].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[1].ComparisonFunc   = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    staticSamplers[1].MaxLOD           = D3D12_FLOAT32_MAX;
    staticSamplers[1].ShaderRegister   = 1;  // s1
    staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters     = 3;
    rootSigDesc.pParameters       = rootParams;
    rootSigDesc.NumStaticSamplers = 2;
    rootSigDesc.pStaticSamplers   = staticSamplers;
    rootSigDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                                  | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                                  | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    Microsoft::WRL::ComPtr<ID3DBlob> serializedSig;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &serializedSig, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, serializedSig->GetBufferPointer(),
                                              serializedSig->GetBufferSize(),
                                              IID_PPV_ARGS(&m_instancedSpriteRootSig))))
        return false;

    D3D12_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode        = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode        = D3D12_CULL_MODE_NONE;
    rastDesc.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = TRUE;
    dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature        = m_instancedSpriteRootSig.Get();
    psoDesc.VS                    = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS                    = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask            = UINT_MAX;
    psoDesc.RasterizerState       = rastDesc;
    psoDesc.DepthStencilState     = dsDesc;
    psoDesc.InputLayout           = { nullptr, 0 };  // procedural vertices via SV_VertexID
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat             = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count      = m_msaaSampleCount;
    psoDesc.SampleDesc.Quality    = 0;

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_instancedSpritePso))))
        return false;

    // Upload buffer: FrameCount regions × kMaxSpriteInstances × 96 bytes per instance
    // (64 bytes mat4 world + 16 bytes vec4 uvRect + 16 bytes vec4 tint)
    {
        const UINT64 bufferSize = static_cast<UINT64>(FrameCount) * kMaxSpriteInstances * 96ull;
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width            = bufferSize;
        rd.Height           = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc.Count = 1;
        rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ,
                                                     nullptr,
                                                     IID_PPV_ARGS(&m_spriteInstanceBuffer))))
            return false;
        D3D12_RANGE readRange = { 0, 0 };
        m_spriteInstanceBuffer->Map(0, &readRange,
                                    reinterpret_cast<void**>(&m_spriteInstanceMapped));
    }

    return true;
}

bool DX12Context::CreateParticlePipeline()
{
    static const char* kShader = R"(
cbuffer PerFrame : register(b0)
{
    row_major float4x4 viewMatrix;
    row_major float4x4 projMatrix;
    row_major float4x4 lightViewProjMatrix;
    float3 cameraEyeWS;
    float  _pad;
};

struct ParticleData
{
    float3 position;
    float  size;
    float4 color;
};

StructuredBuffer<ParticleData> g_particles : register(t0);

static const float2 kQuad[6] =
{
    float2(-0.5f, -0.5f), float2( 0.5f, -0.5f), float2( 0.5f,  0.5f),
    float2(-0.5f, -0.5f), float2( 0.5f,  0.5f), float2(-0.5f,  0.5f)
};

struct VSOut
{
    float4 pos   : SV_POSITION;
    float4 color : COLOR;
    float2 uv    : TEXCOORD;
};

VSOut VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    ParticleData p = g_particles[iid];
    float3 right = float3(viewMatrix[0][0], viewMatrix[0][1], viewMatrix[0][2]);
    float3 up    = float3(viewMatrix[1][0], viewMatrix[1][1], viewMatrix[1][2]);
    float3 wpos  = p.position
                 + right * (kQuad[vid].x * p.size)
                 + up    * (kQuad[vid].y * p.size);
    float4 vpos  = mul(float4(wpos, 1.0f), viewMatrix);
    VSOut o;
    o.pos   = mul(vpos, projMatrix);
    o.color = p.color;
    o.uv    = kQuad[vid] + 0.5f;
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET
{
    float2 uv    = i.uv * 2.0f - 1.0f;
    float  r2    = dot(uv, uv);
    clip(1.0f - r2);
    float  edge  = 1.0f - r2;
    float3 hot   = float3(1.0f, 0.95f, 0.70f);
    float3 col   = lerp(i.color.rgb, hot, edge * edge * 0.5f);
    float  alpha = edge * i.color.a;
    return float4(col, alpha);
}
)";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    if (FAILED(D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
                          "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    errorBlob.Reset();
    if (FAILED(D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
                          "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    // Root sig: param 0 = root CBV b0 (ALL), param 1 = root SRV t0 (VS only)
    D3D12_ROOT_PARAMETER rootParams[2] = {};
    rootParams[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace  = 0;
    rootParams[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParams[1].Descriptor.ShaderRegister = 0;
    rootParams[1].Descriptor.RegisterSpace  = 0;
    rootParams[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 2;
    rsDesc.pParameters   = rootParams;
    rsDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> rsBlob;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &rsBlob, &errorBlob)))
    {
        if (errorBlob) OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, rsBlob->GetBufferPointer(),
                                              rsBlob->GetBufferSize(),
                                              IID_PPV_ARGS(&m_particleRootSig))))
        return false;

    // Additive blending: src*src_alpha + dst*1
    D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {};
    blendDesc.BlendEnable           = TRUE;
    blendDesc.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    blendDesc.DestBlend             = D3D12_BLEND_ONE;
    blendDesc.BlendOp               = D3D12_BLEND_OP_ADD;
    blendDesc.SrcBlendAlpha         = D3D12_BLEND_ONE;
    blendDesc.DestBlendAlpha        = D3D12_BLEND_ZERO;
    blendDesc.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature                   = m_particleRootSig.Get();
    psoDesc.VS                               = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS                               = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState.RenderTarget[0]       = blendDesc;
    psoDesc.SampleMask                       = UINT_MAX;
    psoDesc.RasterizerState.FillMode         = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode         = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable  = TRUE;
    psoDesc.DepthStencilState.DepthEnable    = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.PrimitiveTopologyType            = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets                 = 1;
    psoDesc.RTVFormats[0]                    = DXGI_FORMAT_R16G16B16A16_FLOAT;
    psoDesc.DSVFormat                        = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count                 = m_msaaSampleCount;
    psoDesc.SampleDesc.Quality               = 0;

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_particlePso))))
        return false;

    // Double-buffered upload buffer — 32 bytes per particle (float3 pos, float size, float4 color)
    {
        const UINT64 bufferSize = static_cast<UINT64>(FrameCount) * kMaxParticles * 32ull;
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width            = bufferSize;
        rd.Height           = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc.Count = 1;
        rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(m_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ,
                                                     nullptr,
                                                     IID_PPV_ARGS(&m_particleBuffer))))
            return false;
        D3D12_RANGE readRange = { 0, 0 };
        m_particleBuffer->Map(0, &readRange,
                              reinterpret_cast<void**>(&m_particleMapped));
    }

    return true;
}

bool DX12Context::CreatePostProcessResources()
{
    m_hdrTarget.Reset();
    m_bloomA.Reset();
    m_bloomB.Reset();
    m_postRtvHeap.Reset();
    m_postSrvHeap.Reset();

    m_bloomWidth  = (m_width  > 1) ? m_width  / 2 : 1;
    m_bloomHeight = (m_height > 1) ? m_height / 2 : 1;

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    auto MakeTexDesc = [](UINT64 w, UINT h) {
        D3D12_RESOURCE_DESC d = {};
        d.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width            = w;
        d.Height           = h;
        d.DepthOrArraySize = 1;
        d.MipLevels        = 1;
        d.Format           = DXGI_FORMAT_R16G16B16A16_FLOAT;
        d.SampleDesc.Count = 1;
        d.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        d.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        return d;
    };

    D3D12_RESOURCE_DESC hdrDesc   = MakeTexDesc(m_width,      m_height);
    D3D12_RESOURCE_DESC bloomDesc = MakeTexDesc(m_bloomWidth,  m_bloomHeight);

    auto Create = [&](const D3D12_RESOURCE_DESC& desc, Microsoft::WRL::ComPtr<ID3D12Resource>& out, const wchar_t* name) {
        if (FAILED(m_device->CreateCommittedResource(
                &defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
                IID_PPV_ARGS(&out))))
            return false;
        out->SetName(name);
        return true;
    };

    if (!Create(hdrDesc,   m_hdrTarget, L"HDR Target")) return false;
    if (!Create(bloomDesc, m_bloomA,    L"Bloom A"))    return false;
    if (!Create(bloomDesc, m_bloomB,    L"Bloom B"))    return false;

    // Post-process RTV heap (3 descriptors)
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {};
        hd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = 3;
        if (FAILED(m_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_postRtvHeap))))
            return false;

        const UINT sz = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE h = m_postRtvHeap->GetCPUDescriptorHandleForHeapStart();
        m_device->CreateRenderTargetView(m_hdrTarget.Get(), nullptr, h); h.ptr += sz;
        m_device->CreateRenderTargetView(m_bloomA.Get(),    nullptr, h); h.ptr += sz;
        m_device->CreateRenderTargetView(m_bloomB.Get(),    nullptr, h);
    }

    // Post-process SRV heap (4 slots: hdr, bloomA, bloomB, hdr-duplicate for safe t1 on blur passes)
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {};
        hd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = 4;
        hd.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(m_device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_postSrvHeap))))
            return false;

        const UINT sz = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE h = m_postSrvHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format                  = DXGI_FORMAT_R16G16B16A16_FLOAT;
        srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels     = 1;

        m_device->CreateShaderResourceView(m_hdrTarget.Get(), &srvDesc, h); h.ptr += sz; // slot 0: HDR
        m_device->CreateShaderResourceView(m_bloomA.Get(),    &srvDesc, h); h.ptr += sz; // slot 1: bloomA
        m_device->CreateShaderResourceView(m_bloomB.Get(),    &srvDesc, h); h.ptr += sz; // slot 2: bloomB
        m_device->CreateShaderResourceView(m_hdrTarget.Get(), &srvDesc, h);              // slot 3: HDR dup (safe t1 for blur-V)
    }

    return true;
}

bool DX12Context::CreatePostProcessPipelines()
{
    static const char* kShader = R"(
Texture2D    hdrInput    : register(t0);
Texture2D    bloomInput  : register(t1);
SamplerState linearSmp   : register(s0);
cbuffer PostCB : register(b0) { float2 texelSize; float2 _pad; float fps; float scanlinesEnabled; float ditherEnabled; float frameTimeMs; float entityCount; float drawCallCount; float2 _pad2; };

struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

PSInput VSMain(uint vid : SV_VertexID)
{
    float2 p = float2((vid == 1) ? 3.0f : -1.0f, (vid == 2) ? -3.0f : 1.0f);
    PSInput o;
    o.pos = float4(p, 0.0f, 1.0f);
    o.uv  = float2(p.x * 0.5f + 0.5f, p.y * -0.5f + 0.5f);
    return o;
}

float4 PS_BrightPass(PSInput i) : SV_TARGET
{
    float3 c   = hdrInput.Sample(linearSmp, i.uv).rgb;
    float  lum = dot(c, float3(0.2126f, 0.7152f, 0.0722f));
    float  thr = 0.80f, knee = 0.20f;
    float  rq  = clamp(lum - thr + knee, 0.0f, 2.0f * knee);
    rq = (rq * rq) / (4.0f * knee + 1e-5f);
    float  w   = max(rq, lum - thr) / max(lum, 1e-5f);
    return float4(c * w, 1.0f);
}

static const float kW[5] = { 0.227027f, 0.194595f, 0.121622f, 0.054054f, 0.016216f };

float4 PS_BlurH(PSInput i) : SV_TARGET
{
    float3 r = hdrInput.Sample(linearSmp, i.uv).rgb * kW[0];
    [unroll] for (int j = 1; j <= 4; ++j) {
        float2 o = float2(texelSize.x * j, 0.0f);
        r += hdrInput.Sample(linearSmp, i.uv + o).rgb * kW[j];
        r += hdrInput.Sample(linearSmp, i.uv - o).rgb * kW[j];
    }
    return float4(r, 1.0f);
}

float4 PS_BlurV(PSInput i) : SV_TARGET
{
    float3 r = hdrInput.Sample(linearSmp, i.uv).rgb * kW[0];
    [unroll] for (int j = 1; j <= 4; ++j) {
        float2 o = float2(0.0f, texelSize.y * j);
        r += hdrInput.Sample(linearSmp, i.uv + o).rgb * kW[j];
        r += hdrInput.Sample(linearSmp, i.uv - o).rgb * kW[j];
    }
    return float4(r, 1.0f);
}

float3 ACESFilm(float3 x) { return saturate((x*(2.51f*x+0.03f))/(x*(2.43f*x+0.59f)+0.14f)); }

int GetDigitBitmap(int d)
{
    if (d == 0) return 31599;
    if (d == 1) return 11415;
    if (d == 2) return 29671;
    if (d == 3) return 29647;
    if (d == 4) return 23497;
    if (d == 5) return 31183;
    if (d == 6) return 31215;
    if (d == 7) return 29257;
    if (d == 8) return 31727;
    if (d == 9) return 31695;
    return 0;
}

// Sample one pixel of a numDigits-wide integer display; relX/relY are relative to the number's top-left
float SampleDigits(int value, int numDigits, int relX, int relY)
{
    const int SCALE  = 4;
    const int CHAR_W = 3 * SCALE;
    const int GAP    = 3;
    const int SLOT   = CHAR_W + GAP;
    const int CHAR_H = 5 * SCALE;
    if (relX < 0 || relY < 0 || relY >= CHAR_H || relX >= numDigits * SLOT - GAP)
        return 0.0f;
    int charIdx = relX / SLOT;
    int xInSlot = relX - charIdx * SLOT;
    if (xInSlot >= CHAR_W || charIdx >= numDigits)
        return 0.0f;
    int exp = numDigits - charIdx - 1;
    int div = (exp == 4) ? 10000 : (exp == 3) ? 1000 : (exp == 2) ? 100 : (exp == 1) ? 10 : 1;
    int d   = (value / div) % 10;
    int col = xInSlot / SCALE;
    int row = relY / SCALE;
    int bmp = GetDigitBitmap(d);
    return float((bmp >> (14 - row * 3 - col)) & 1);
}

float4 PS_Tonemap(PSInput i) : SV_TARGET
{
    float3 hdr   = hdrInput.Sample(linearSmp,  i.uv).rgb;
    float3 bloom = bloomInput.Sample(linearSmp, i.uv).rgb;
    float3 color = ACESFilm(hdr + bloom * 0.07f);

    // Scanlines + phosphor pixel grid (C key)
    if (scanlinesEnabled > 0.5f)
    {
        // Every other row darkened to 55%
        float scanline = (fmod(i.pos.y, 2.0f) < 1.0f) ? 1.0f : 0.55f;
        // Subtle brightness dip between pixel columns — creates a pixel-grid look
        float phosphor = 0.80f + 0.20f * sin(i.pos.x * 3.14159265f);
        color *= scanline * phosphor;
    }

    // Bayer 4x4 ordered dithering — quantises to 8 levels per channel (V key)
    if (ditherEnabled > 0.5f)
    {
        static const int kBayer[16] = {
             0,  8,  2, 10,
            12,  4, 14,  6,
             3, 11,  1,  9,
            15,  7, 13,  5
        };
        int crtPx = int(i.pos.x);
        int crtPy = int(i.pos.y);
        float bayerT = float(kBayer[(crtPy % 4) * 4 + (crtPx % 4)]) / 16.0f;
        color = saturate(floor(color * 8.0f + bayerT) / 8.0f);
    }

    // Stats overlay — 4 rows, dark box, top-left corner
    // Row 0 yellow : FPS          (3 digits, 0-999)
    // Row 1 cyan   : Frame time   (3 digits, ms)
    // Row 2 green  : Entity count (5 digits)
    // Row 3 orange : Draw calls   (5 digits)
    {
        const int OX    = 8;
        const int OY    = 8;
        const int ROW_H = 24;  // 20px char + 4px gap
        const int BOX_W = 80;
        const int BOX_H = 4 * ROW_H - 4; // 92px — no trailing gap
        int spx = (int)i.pos.x - OX;
        int spy = (int)i.pos.y - OY;
        if (spx >= 0 && spx < BOX_W && spy >= 0 && spy < BOX_H)
        {
            color *= 0.20f;
            float lit;
            lit = SampleDigits(clamp((int)fps,           0, 999),   3, spx, spy - 0 * ROW_H);
            if (lit > 0.5f) color = float3(1.00f, 0.95f, 0.00f); // yellow  — FPS
            lit = SampleDigits(clamp((int)frameTimeMs,   0, 999),   3, spx, spy - 1 * ROW_H);
            if (lit > 0.5f) color = float3(0.00f, 0.90f, 1.00f); // cyan    — ms
            lit = SampleDigits(clamp((int)entityCount,   0, 99999), 5, spx, spy - 2 * ROW_H);
            if (lit > 0.5f) color = float3(0.30f, 1.00f, 0.30f); // green   — entities
            lit = SampleDigits(clamp((int)drawCallCount, 0, 99999), 5, spx, spy - 3 * ROW_H);
            if (lit > 0.5f) color = float3(1.00f, 0.60f, 0.00f); // orange  — draw calls
        }
    }

    return float4(color, 1.0f);
}
)";

    UINT flags = 0;
#if defined(_DEBUG)
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, bpBlob, bhBlob, bvBlob, tmBlob, errBlob;

    auto Compile = [&](LPCSTR entry, LPCSTR target,
                       Microsoft::WRL::ComPtr<ID3DBlob>& out) -> bool
    {
        errBlob.Reset();
        HRESULT hr = D3DCompile(kShader, strlen(kShader), nullptr, nullptr, nullptr,
                                entry, target, flags, 0, &out, &errBlob);
        if (FAILED(hr)) {
            if (errBlob) OutputDebugStringA(static_cast<const char*>(errBlob->GetBufferPointer()));
            return false;
        }
        return true;
    };

    if (!Compile("VSMain",        "vs_5_0", vsBlob)) return false;
    if (!Compile("PS_BrightPass", "ps_5_0", bpBlob)) return false;
    if (!Compile("PS_BlurH",      "ps_5_0", bhBlob)) return false;
    if (!Compile("PS_BlurV",      "ps_5_0", bvBlob)) return false;
    if (!Compile("PS_Tonemap",    "ps_5_0", tmBlob)) return false;

    // Root signature: 4 root constants (b0 blur texel size) + descriptor table (2 SRVs t0/t1)
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors     = 2;
    srvRange.BaseShaderRegister = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[2] = {};
    rootParams[0].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants.Num32BitValues = 12;
    rootParams[0].Constants.ShaderRegister = 0;
    rootParams[0].ShaderVisibility         = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges   = &srvRange;
    rootParams[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC samp = {};
    samp.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samp.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
    samp.MaxLOD           = D3D12_FLOAT32_MAX;
    samp.ShaderRegister   = 0;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC sigDesc = {};
    sigDesc.NumParameters     = 2;
    sigDesc.pParameters       = rootParams;
    sigDesc.NumStaticSamplers = 1;
    sigDesc.pStaticSamplers   = &samp;

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    if (FAILED(D3D12SerializeRootSignature(&sigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob))) {
        if (errBlob) OutputDebugStringA(static_cast<const char*>(errBlob->GetBufferPointer()));
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(),
                                             sigBlob->GetBufferSize(),
                                             IID_PPV_ARGS(&m_postRootSig))))
        return false;

    // Shared PSO template (no IA, no depth, no MSAA, fullscreen triangle)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = m_postRootSig.Get();
    pso.VS             = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.SampleMask                     = UINT_MAX;
    pso.RasterizerState.FillMode       = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode       = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = FALSE;
    pso.DepthStencilState.DepthEnable   = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets      = 1;
    pso.SampleDesc.Count      = 1;

    // Bright pass + blur → R16G16B16A16_FLOAT (half-res bloom targets)
    pso.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;

    pso.PS = { bpBlob->GetBufferPointer(), bpBlob->GetBufferSize() };
    if (FAILED(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_brightPassPso)))) return false;

    pso.PS = { bhBlob->GetBufferPointer(), bhBlob->GetBufferSize() };
    if (FAILED(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_blurHPso)))) return false;

    pso.PS = { bvBlob->GetBufferPointer(), bvBlob->GetBufferSize() };
    if (FAILED(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_blurVPso)))) return false;

    // Tonemap → swap chain format (R8G8B8A8_UNORM)
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.PS = { tmBlob->GetBufferPointer(), tmBlob->GetBufferSize() };
    if (FAILED(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_tonemapPso)))) return false;

    return true;
}

void DX12Context::WaitForGpu()
{
    if (!m_commandQueue || !m_fence)
        return;

    const uint64_t fenceValue = m_fenceValues[m_frameIndex];
    if (FAILED(m_commandQueue->Signal(m_fence.Get(), fenceValue)))
        return;

    if (m_fence->GetCompletedValue() < fenceValue)
    {
        if (FAILED(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent)))
            return;

        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_fenceValues[m_frameIndex] += 1;
}

void DX12Context::MoveToNextFrame()
{
    if (!m_commandQueue || !m_fence || !m_swapChain)
        return;

    const uint64_t currentFenceValue = m_fenceValues[m_frameIndex];

    if (FAILED(m_commandQueue->Signal(m_fence.Get(), currentFenceValue)))
        return;

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        if (FAILED(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent)))
            return;

        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void DX12Context::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_width = width;
    m_height = height;

    WaitForGpu();

    for (uint32_t i = 0; i < FrameCount; ++i)
    {
        m_renderTargets[i].Reset();
    }
    m_depthStencil.Reset();

    if (FAILED(m_swapChain->ResizeBuffers(FrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0)))
        return;

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    CreateRenderTargetViews();
    CreateDepthStencilBuffer();
    CreatePostProcessResources();
}

void DX12Context::BeginFrame()
{
    if (FAILED(m_commandAllocators[m_frameIndex]->Reset()))
        return;

    if (FAILED(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr)))
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE msaaRtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    msaaRtvHandle.ptr += FrameCount * m_rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    const FLOAT clearColor[4] = {0.46f, 0.56f, 0.69f, 1.0f};
    m_commandList->OMSetRenderTargets(1, &msaaRtvHandle, FALSE, &dsvHandle);
    m_commandList->ClearRenderTargetView(msaaRtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(m_width);
    scissorRect.bottom = static_cast<LONG>(m_height);

    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &scissorRect);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DX12Context::RenderScene(Scene *scene, const Camera &camera)
{
    if (!scene)
        return;

    m_entityCount   = static_cast<uint32_t>(scene->GetEntities().size());
    m_drawCallCount = 0;

    // ---- Shadow pass -------------------------------------------------------
    // Shadow map enters RenderScene in DEPTH_WRITE (initial or restored at end of previous frame)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE shadowDsv = m_shadowDsvHeap->GetCPUDescriptorHandleForHeapStart();
        m_commandList->OMSetRenderTargets(0, nullptr, FALSE, &shadowDsv);
        m_commandList->ClearDepthStencilView(shadowDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        D3D12_VIEWPORT shadowVp = { 0.0f, 0.0f,
            static_cast<float>(kShadowMapSize), static_cast<float>(kShadowMapSize), 0.0f, 1.0f };
        D3D12_RECT shadowScissor = { 0, 0,
            static_cast<LONG>(kShadowMapSize), static_cast<LONG>(kShadowMapSize) };
        m_commandList->RSSetViewports(1, &shadowVp);
        m_commandList->RSSetScissorRects(1, &shadowScissor);

        struct ShadowCB       { mat4 world; mat4 lightVP; };
        struct ShadowSpriteCB { mat4 world; mat4 lightVP; vec4 uvRect; };

        for (auto &entity : scene->GetEntities())
        {
            if (!entity || !entity->enabled || !entity->castsProjectedShadow || !entity->mesh)
                continue;

            if (entity->isBillboardActor)
            {
                if (!entity->material || !entity->material->GetSrvHeap())
                    continue;

                m_commandList->SetPipelineState(m_shadowSpritePso.Get());
                m_commandList->SetGraphicsRootSignature(m_shadowSpriteRootSig.Get());

                ID3D12DescriptorHeap* heap = entity->material->GetSrvHeap();
                m_commandList->SetDescriptorHeaps(1, &heap);
                m_commandList->SetGraphicsRootDescriptorTable(1, heap->GetGPUDescriptorHandleForHeapStart());

                ShadowSpriteCB scb;
                scb.world   = MatrixBillboard(entity->transform.position, entity->transform.scale, m_lightPos);
                scb.lightVP = m_lightViewProj;
                scb.uvRect  = entity->spriteUVRect;
                m_commandList->SetGraphicsRoot32BitConstants(0, sizeof(scb) / 4, &scb, 0);
            }
            else
            {
                m_commandList->SetPipelineState(m_shadowPso.Get());
                m_commandList->SetGraphicsRootSignature(m_shadowRootSig.Get());

                ShadowCB cb;
                cb.world   = entity->transform.GetWorldMatrix();
                cb.lightVP = m_lightViewProj;
                m_commandList->SetGraphicsRoot32BitConstants(0, sizeof(cb) / 4, &cb, 0);
            }

            entity->mesh->Draw(m_commandList.Get());
            ++m_drawCallCount;
        }

        D3D12_RESOURCE_BARRIER toSrv = {};
        toSrv.Type                       = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toSrv.Transition.pResource       = m_shadowMap.Get();
        toSrv.Transition.StateBefore     = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        toSrv.Transition.StateAfter      = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toSrv.Transition.Subresource     = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &toSrv);
    }

    // ---- Main pass ---------------------------------------------------------
    {
        D3D12_CPU_DESCRIPTOR_HANDLE msaaRtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        msaaRtv.ptr += FrameCount * m_rtvDescriptorSize;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
        m_commandList->OMSetRenderTargets(1, &msaaRtv, FALSE, &dsv);

        D3D12_VIEWPORT vp = { 0.0f, 0.0f,
            static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
        D3D12_RECT scissor = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
        m_commandList->RSSetViewports(1, &vp);
        m_commandList->RSSetScissorRects(1, &scissor);

        const float aspect = (m_height > 0)
            ? (static_cast<float>(m_width) / static_cast<float>(m_height))
            : (16.0f / 9.0f);

        FrameCameraData frameData;
        frameData.viewMatrix          = MatrixLookAtRH(camera.eye, camera.target, vec3(0.0f, 1.0f, 0.0f));
        frameData.projMatrix          = MatrixPerspectiveRH(camera.fovY, aspect, camera.nearZ, camera.farZ);
        frameData.lightViewProjMatrix = m_lightViewProj;

        // Write per-frame data into the current frame's slot of the upload CB
        struct PerFrameCbData { mat4 view; mat4 proj; mat4 lightVP; vec3 cameraEye; float pad; };
        PerFrameCbData cbData;
        cbData.view      = frameData.viewMatrix;
        cbData.proj      = frameData.projMatrix;
        cbData.lightVP   = frameData.lightViewProjMatrix;
        cbData.cameraEye = camera.eye;
        cbData.pad       = 0.0f;
        memcpy(m_perFrameCbMapped + 256 * m_frameIndex, &cbData, sizeof(cbData));
        frameData.perFrameGpuAddr = m_perFrameCb->GetGPUVirtualAddress() + 256 * m_frameIndex;

        // Per-instance data layout must match the HLSL SpriteInstance struct (96 bytes)
        struct SpriteInstanceData { mat4 world; vec4 uvRect; vec4 tint; };

        SpriteInstanceData* instanceDst = reinterpret_cast<SpriteInstanceData*>(m_spriteInstanceMapped)
                                          + m_frameIndex * kMaxSpriteInstances;
        uint32_t  instanceCount     = 0;
        Material* instancedMaterial = nullptr;

        // Extract frustum planes from combined VP for this frame
        FrustumPlane frustum[6];
        ExtractFrustumPlanes(MatrixMultiply(frameData.viewMatrix, frameData.projMatrix), frustum);

        m_instanceSortBuf.clear();
        for (auto &entity : scene->GetEntities())
        {
            if (!entity || !entity->enabled)
                continue;

            if (entity->useInstancing)
            {
                // Frustum cull: skip sprites whose centre is outside any plane
                const vec3& pos = entity->transform.position;
                if (!PointInFrustum(frustum, pos.x, pos.y, pos.z))
                    continue;

                // Squared distance for front-to-back sort (no sqrt — only ordering matters)
                const vec3 d = pos - camera.eye;
                m_instanceSortBuf.push_back({ d.x*d.x + d.y*d.y + d.z*d.z, entity.get() });
                if (!instancedMaterial) instancedMaterial = entity->material.get();
                continue;
            }

            if (entity->isBillboardActor)
            {
                const mat4 billboardWorld = MatrixBillboard(
                    entity->transform.position, entity->transform.scale, camera.eye);
                entity->Draw(m_commandList.Get(), frameData, &billboardWorld, nullptr, false);
                ++m_drawCallCount;
                continue;
            }

            entity->Draw(m_commandList.Get(), frameData);
            ++m_drawCallCount;
        }

        // Sort front-to-back so early-Z discards distant sprites that are hidden by closer ones
        std::sort(m_instanceSortBuf.begin(), m_instanceSortBuf.end(),
            [](const std::pair<float, const Entity*>& a,
               const std::pair<float, const Entity*>& b) { return a.first < b.first; });

        instanceCount = static_cast<uint32_t>(
            std::min(m_instanceSortBuf.size(), static_cast<size_t>(kMaxSpriteInstances)));
        for (uint32_t i = 0; i < instanceCount; ++i)
        {
            const Entity* e = m_instanceSortBuf[i].second;
            instanceDst[i].world  = MatrixBillboard(
                e->transform.position, e->transform.scale, camera.eye);
            instanceDst[i].uvRect = e->spriteUVRect;
            instanceDst[i].tint   = e->tint;
        }

        // Single instanced draw replaces the 5000 individual draws
        if (instanceCount > 0 && instancedMaterial && instancedMaterial->GetSrvHeap())
        {
            ID3D12DescriptorHeap* heap = instancedMaterial->GetSrvHeap();
            m_commandList->SetDescriptorHeaps(1, &heap);
            m_commandList->SetPipelineState(m_instancedSpritePso.Get());
            m_commandList->SetGraphicsRootSignature(m_instancedSpriteRootSig.Get());

            // Param 0: perFrame CB (view, proj, cameraEye, etc.)
            m_commandList->SetGraphicsRootConstantBufferView(0, frameData.perFrameGpuAddr);

            // Param 1: root SRV pointing to this frame's region of the instance buffer
            const D3D12_GPU_VIRTUAL_ADDRESS instAddr =
                m_spriteInstanceBuffer->GetGPUVirtualAddress()
                + static_cast<UINT64>(m_frameIndex) * kMaxSpriteInstances * 96ull;
            m_commandList->SetGraphicsRootShaderResourceView(1, instAddr);

            // Param 2: descriptor table — heap slot 0 (sprite atlas) → shader register t1
            m_commandList->SetGraphicsRootDescriptorTable(
                2, heap->GetGPUDescriptorHandleForHeapStart());

            m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_commandList->DrawInstanced(6, instanceCount, 0, 0);
            ++m_drawCallCount;
        }

        // Fire particle pass — additive billboards, depth test on, no depth write
        {
            const auto&    sceneParts = scene->GetParticles();
            const uint32_t pCount     = static_cast<uint32_t>(
                std::min(sceneParts.size(), static_cast<size_t>(kMaxParticles)));
            if (pCount > 0 && m_particlePso && m_particleBuffer)
            {
                uint8_t* dst = m_particleMapped
                             + static_cast<size_t>(m_frameIndex) * kMaxParticles * 32;
                memcpy(dst, sceneParts.data(), pCount * 32);

                m_commandList->SetPipelineState(m_particlePso.Get());
                m_commandList->SetGraphicsRootSignature(m_particleRootSig.Get());
                m_commandList->SetGraphicsRootConstantBufferView(0, frameData.perFrameGpuAddr);
                const D3D12_GPU_VIRTUAL_ADDRESS pAddr =
                    m_particleBuffer->GetGPUVirtualAddress()
                    + static_cast<UINT64>(m_frameIndex) * kMaxParticles * 32ull;
                m_commandList->SetGraphicsRootShaderResourceView(1, pAddr);
                m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                m_commandList->DrawInstanced(6, pCount, 0, 0);
                ++m_drawCallCount;
            }
        }
    }

    // Restore shadow map to DEPTH_WRITE so next frame's shadow pass can clear it without a barrier
    {
        D3D12_RESOURCE_BARRIER toDepth = {};
        toDepth.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toDepth.Transition.pResource   = m_shadowMap.Get();
        toDepth.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        toDepth.Transition.StateAfter  = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        toDepth.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &toDepth);
    }
}

void DX12Context::EndFrame()
{
    // Helper: fill a transition barrier inline
    auto Trans = [](ID3D12Resource* r, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER b = {};
        b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource   = r;
        b.Transition.StateBefore = before;
        b.Transition.StateAfter  = after;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        return b;
    };

    const UINT rtvSz = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const UINT srvSz = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE hdrRtv   = m_postRtvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE bloomARtv = { hdrRtv.ptr + rtvSz };
    D3D12_CPU_DESCRIPTOR_HANDLE bloomBRtv = { hdrRtv.ptr + rtvSz * 2 };

    D3D12_GPU_DESCRIPTOR_HANDLE hdrSrv   = m_postSrvHeap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE bloomASrv = { hdrSrv.ptr + srvSz };
    D3D12_GPU_DESCRIPTOR_HANDLE bloomBSrv = { hdrSrv.ptr + srvSz * 2 };

    // 1. Resolve MSAA → HDR target (R16G16B16A16_FLOAT)
    {
        D3D12_RESOURCE_BARRIER b[2] = {
            Trans(m_msaaColorTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,        D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
            Trans(m_hdrTarget.Get(),       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST),
        };
        m_commandList->ResourceBarrier(2, b);
    }
    m_commandList->ResolveSubresource(m_hdrTarget.Get(), 0,
                                      m_msaaColorTarget.Get(), 0,
                                      DXGI_FORMAT_R16G16B16A16_FLOAT);
    {
        D3D12_RESOURCE_BARRIER b[2] = {
            Trans(m_msaaColorTarget.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
            Trans(m_hdrTarget.Get(),       D3D12_RESOURCE_STATE_RESOLVE_DEST,   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        };
        m_commandList->ResourceBarrier(2, b);
    }

    // Bind post-process state
    ID3D12DescriptorHeap* postHeaps[] = { m_postSrvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, postHeaps);
    m_commandList->SetGraphicsRootSignature(m_postRootSig.Get());

    auto DrawFS = [&](float w, float h)
    {
        D3D12_VIEWPORT vp     = { 0.0f, 0.0f, w, h, 0.0f, 1.0f };
        D3D12_RECT     scissor = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
        m_commandList->RSSetViewports(1, &vp);
        m_commandList->RSSetScissorRects(1, &scissor);
        m_commandList->DrawInstanced(3, 1, 0, 0);
    };

    const float fw = static_cast<float>(m_width);
    const float fh = static_cast<float>(m_height);
    const float bw = static_cast<float>(m_bloomWidth);
    const float bh = static_cast<float>(m_bloomHeight);

    // 2. Bright-pass: HDR → bloomA
    {
        D3D12_RESOURCE_BARRIER b = Trans(m_bloomA.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &b);
    }
    m_commandList->SetPipelineState(m_brightPassPso.Get());
    m_commandList->OMSetRenderTargets(1, &bloomARtv, FALSE, nullptr);
    m_commandList->SetGraphicsRootDescriptorTable(1, hdrSrv); // t0=HDR
    DrawFS(bw, bh);
    {
        D3D12_RESOURCE_BARRIER b = Trans(m_bloomA.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &b);
    }

    // 3. Horizontal blur: bloomA → bloomB
    {
        D3D12_RESOURCE_BARRIER b = Trans(m_bloomB.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &b);
    }
    {
        float tc[4] = { 1.0f / bw, 0.0f, 0.0f, 0.0f };
        m_commandList->SetGraphicsRoot32BitConstants(0, 4, tc, 0);
    }
    m_commandList->SetPipelineState(m_blurHPso.Get());
    m_commandList->OMSetRenderTargets(1, &bloomBRtv, FALSE, nullptr);
    m_commandList->SetGraphicsRootDescriptorTable(1, bloomASrv); // t0=bloomA
    DrawFS(bw, bh);
    {
        D3D12_RESOURCE_BARRIER b = Trans(m_bloomB.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &b);
    }

    // 4. Vertical blur: bloomB → bloomA
    {
        D3D12_RESOURCE_BARRIER b = Trans(m_bloomA.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &b);
    }
    {
        float tc[4] = { 0.0f, 1.0f / bh, 0.0f, 0.0f };
        m_commandList->SetGraphicsRoot32BitConstants(0, 4, tc, 0);
    }
    m_commandList->SetPipelineState(m_blurVPso.Get());
    m_commandList->OMSetRenderTargets(1, &bloomARtv, FALSE, nullptr);
    m_commandList->SetGraphicsRootDescriptorTable(1, bloomBSrv); // t0=bloomB
    DrawFS(bw, bh);
    {
        D3D12_RESOURCE_BARRIER b = Trans(m_bloomA.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &b);
    }

    // 5. Tonemap + composite → swap chain (HDR t0, bloomA t1)
    {
        D3D12_RESOURCE_BARRIER b = Trans(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &b);
    }
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;
        m_commandList->SetPipelineState(m_tonemapPso.Get());
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        m_commandList->SetGraphicsRootDescriptorTable(1, hdrSrv); // t0=HDR, t1=bloomA (contiguous)
        float scanlinesVal   = m_scanlinesEnabled ? 1.0f : 0.0f;
        float ditherVal      = m_ditherEnabled    ? 1.0f : 0.0f;
        float frameTimeMs    = (m_fps > 0.0f) ? 1000.0f / m_fps : 0.0f;
        float entityCountF   = static_cast<float>(m_entityCount);
        float drawCallCountF = static_cast<float>(m_drawCallCount);
        m_commandList->SetGraphicsRoot32BitConstants(0, 1, &m_fps,          4);
        m_commandList->SetGraphicsRoot32BitConstants(0, 1, &scanlinesVal,   5);
        m_commandList->SetGraphicsRoot32BitConstants(0, 1, &ditherVal,      6);
        m_commandList->SetGraphicsRoot32BitConstants(0, 1, &frameTimeMs,    7);
        m_commandList->SetGraphicsRoot32BitConstants(0, 1, &entityCountF,   8);
        m_commandList->SetGraphicsRoot32BitConstants(0, 1, &drawCallCountF, 9);
        DrawFS(fw, fh);
    }
    {
        D3D12_RESOURCE_BARRIER b = Trans(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &b);
    }

    if (FAILED(m_commandList->Close()))
        return;

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    m_swapChain->Present(1, 0);
    MoveToNextFrame();
}
