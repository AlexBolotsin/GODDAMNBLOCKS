#include "DX12Context.h"
#include "Scene.h"
#include "Camera.h"
#include <cmath>
#include <cstdio>

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

}

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
        DXGI_FORMAT_R8G8B8A8_UNORM,
        m_depthStencilFormat,
        requestedMsaaSampleCount);

    LogMsaaSupport(
        m_device.Get(),
        DXGI_FORMAT_R8G8B8A8_UNORM,
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
    msaaDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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

    const float aspect = (m_height > 0) ? (static_cast<float>(m_width) / static_cast<float>(m_height)) : (16.0f / 9.0f);
    FrameCameraData frameData;
    frameData.viewMatrix = MatrixLookAtRH(camera.eye, camera.target, vec3(0.0f, 1.0f, 0.0f));
    frameData.projMatrix = MatrixPerspectiveRH(camera.fovY, aspect, camera.nearZ, camera.farZ);

    for (auto &entity : scene->GetEntities())
    {
        if (!entity)
            continue;

        if (entity->isBillboardActor)
        {
            const mat4 billboardWorld = MatrixBillboard(entity->transform.position, entity->transform.scale, camera.eye);
            entity->Draw(m_commandList.Get(), frameData, &billboardWorld, nullptr, false);
            continue;
        }

        entity->Draw(m_commandList.Get(), frameData);
    }

    const float groundPlaneY = -0.985f;
    const vec3 shadowRayDir = Vec3Normalize(vec3(-0.40f, -0.95f, -0.55f));
    const mat4 shadowProj = MatrixShadowProjection(groundPlaneY, shadowRayDir);

    const vec4 shadowTints[] =
        {
            vec4(0.0f, 0.0f, 0.0f, 0.16f),
            vec4(0.0f, 0.0f, 0.0f, 0.08f),
            vec4(0.0f, 0.0f, 0.0f, 0.08f),
        };
    const vec3 shadowOffsets[] =
        {
            vec3(0.0f, 0.0015f, 0.0f),
            vec3(0.06f, 0.0030f, 0.03f),
            vec3(-0.05f, 0.0045f, -0.02f),
        };

    for (size_t i = 1; i < scene->GetEntities().size(); ++i)
    {
        Entity *entity = scene->GetEntities()[i].get();
        if (!entity || entity->isBillboardActor || !entity->castsProjectedShadow)
            continue;

        const mat4 objectWorld = entity->transform.GetWorldMatrix();
        const mat4 projectedWorld = MatrixMultiply(objectWorld, shadowProj);

        for (int s = 0; s < 3; ++s)
        {
            const mat4 offset = MatrixTranslation(shadowOffsets[s].x, shadowOffsets[s].y, shadowOffsets[s].z);
            const mat4 shadowWorld = MatrixMultiply(projectedWorld, offset);
            entity->Draw(m_commandList.Get(), frameData, &shadowWorld, &shadowTints[s], true);
        }
    }
}

void DX12Context::EndFrame()
{
    D3D12_RESOURCE_BARRIER barriers[2] = {};

    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[0].Transition.pResource = m_msaaColorTarget.Get();
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[1].Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_DEST;

    m_commandList->ResourceBarrier(2, barriers);

    m_commandList->ResolveSubresource(
        m_renderTargets[m_frameIndex].Get(),
        0,
        m_msaaColorTarget.Get(),
        0,
        DXGI_FORMAT_R8G8B8A8_UNORM);

    D3D12_RESOURCE_BARRIER postBarriers[2] = {};

    postBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    postBarriers[0].Transition.pResource = m_renderTargets[m_frameIndex].Get();
    postBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
    postBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    postBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    postBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    postBarriers[1].Transition.pResource = m_msaaColorTarget.Get();
    postBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    postBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    postBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_commandList->ResourceBarrier(2, postBarriers);

    if (FAILED(m_commandList->Close()))
        return;

    ID3D12CommandList *commandLists[] = {m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    m_swapChain->Present(1, 0);
    MoveToNextFrame();
}
