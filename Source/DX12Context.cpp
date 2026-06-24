#include "DX12Context.h"
#include "Scene.h"
#include <cmath>

namespace
{
    vec3 Vec3Sub(const vec3& a, const vec3& b)
    {
        return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    float Vec3Dot(const vec3& a, const vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    vec3 Vec3Cross(const vec3& a, const vec3& b)
    {
        return vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
    }

    vec3 Vec3Normalize(const vec3& v)
    {
        const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq <= 1e-12f)
            return vec3(0.0f, 0.0f, 0.0f);

        const float invLen = 1.0f / sqrtf(lenSq);
        return vec3(v.x * invLen, v.y * invLen, v.z * invLen);
    }

    mat4 BuildLookAtRH(const vec3& eye, const vec3& target, const vec3& up)
    {
        const vec3 zAxis = Vec3Normalize(Vec3Sub(eye, target));
        const vec3 xAxis = Vec3Normalize(Vec3Cross(up, zAxis));
        const vec3 yAxis = Vec3Cross(zAxis, xAxis);

        mat4 view;
        view.m[0] = xAxis.x;
        view.m[1] = yAxis.x;
        view.m[2] = zAxis.x;
        view.m[3] = 0.0f;

        view.m[4] = xAxis.y;
        view.m[5] = yAxis.y;
        view.m[6] = zAxis.y;
        view.m[7] = 0.0f;

        view.m[8] = xAxis.z;
        view.m[9] = yAxis.z;
        view.m[10] = zAxis.z;
        view.m[11] = 0.0f;

        view.m[12] = -Vec3Dot(xAxis, eye);
        view.m[13] = -Vec3Dot(yAxis, eye);
        view.m[14] = -Vec3Dot(zAxis, eye);
        view.m[15] = 1.0f;

        return view;
    }

    mat4 BuildPerspectiveRH(float fovYRadians, float aspect, float nearZ, float farZ)
    {
        mat4 result;

        const float f = 1.0f / tanf(fovYRadians * 0.5f);
        result.m[0] = f / aspect;
        result.m[5] = f;
        result.m[10] = farZ / (nearZ - farZ);
        result.m[11] = -1.0f;
        result.m[14] = nearZ * farZ / (nearZ - farZ);
        result.m[15] = 0.0f;

        return result;
    }

    mat4 BuildDirectionalShadowProjection(float planeY, const vec3& rayDir)
    {
        mat4 shadow;

        const float safeY = (fabsf(rayDir.y) > 1e-4f) ? rayDir.y : -1e-4f;
        const float kx = rayDir.x / safeY;
        const float kz = rayDir.z / safeY;

        shadow.m[0] = 1.0f;
        shadow.m[4] = -kx;
        shadow.m[8] = 0.0f;
        shadow.m[12] = kx * planeY;

        shadow.m[1] = 0.0f;
        shadow.m[5] = 0.0f;
        shadow.m[9] = 0.0f;
        shadow.m[13] = planeY;

        shadow.m[2] = 0.0f;
        shadow.m[6] = -kz;
        shadow.m[10] = 1.0f;
        shadow.m[14] = kz * planeY;

        shadow.m[3] = 0.0f;
        shadow.m[7] = 0.0f;
        shadow.m[11] = 0.0f;
        shadow.m[15] = 1.0f;

        return shadow;
    }

    mat4 BuildBillboardWorld(const vec3& position, const vec3& scale, const vec3& cameraEye)
    {
        vec3 toCamera = Vec3Normalize(Vec3Sub(cameraEye, position));
        if (toCamera.x == 0.0f && toCamera.y == 0.0f && toCamera.z == 0.0f)
            toCamera = vec3(0.0f, 0.0f, 1.0f);

        const vec3 worldUp(0.0f, 1.0f, 0.0f);
        vec3 right = Vec3Cross(worldUp, toCamera);
        if (Vec3Dot(right, right) <= 1e-8f)
            right = vec3(1.0f, 0.0f, 0.0f);
        else
            right = Vec3Normalize(right);

        vec3 up = Vec3Normalize(Vec3Cross(toCamera, right));

        mat4 world;
        world.m[0] = right.x * scale.x;
        world.m[1] = right.y * scale.x;
        world.m[2] = right.z * scale.x;
        world.m[3] = 0.0f;

        world.m[4] = up.x * scale.y;
        world.m[5] = up.y * scale.y;
        world.m[6] = up.z * scale.y;
        world.m[7] = 0.0f;

        world.m[8] = toCamera.x * scale.z;
        world.m[9] = toCamera.y * scale.z;
        world.m[10] = toCamera.z * scale.z;
        world.m[11] = 0.0f;

        world.m[12] = position.x;
        world.m[13] = position.y;
        world.m[14] = position.z;
        world.m[15] = 1.0f;

        return world;
    }
}

void DX12Context::SetCamera(const vec3& eye, const vec3& target)
{
    m_cameraEye = eye;
    m_cameraTarget = target;
}


DX12Context::~DX12Context()
{
    Shutdown();
}

bool DX12Context::Init(HWND hwnd, uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;

    if (!CreateDevice())
        return false;

    if (!CreateCommandObjects())
        return false;

    if (!CreateSwapChain(hwnd, width, height))
        return false;

    if (!CreateRenderTargetViews())
        return false;

    if (!CreateDepthStencilBuffer())
        return false;

    if (!CreateFenceAndEvent())
        return false;

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
    m_depthStencil.Reset();
    m_swapChain.Reset();
    m_commandQueue.Reset();
    m_fence.Reset();
    m_device.Reset();
    m_dxgiFactory.Reset();
}

bool DX12Context::CreateDevice()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory))))
        return false;

    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
        if (FAILED(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter))))
            return false;

        if (FAILED(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
            return false;
    }

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
    rtvHeapDesc.NumDescriptors = FrameCount;
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

    return true;
}

bool DX12Context::CreateDepthStencilBuffer()
{
    if (!m_dsvHeap)
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap))))
            return false;
    }

    m_depthStencil.Reset();

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Alignment = 0;
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = m_depthStencilFormat;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = m_depthStencilFormat;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    if (FAILED(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_depthStencil))))
    {
        return false;
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = m_depthStencilFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

bool DX12Context::CreateFenceAndEvent()
{
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence))))
        return false;

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

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += m_frameIndex * m_rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    const FLOAT clearColor[4] = { 0.46f, 0.56f, 0.69f, 1.0f };
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
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

void DX12Context::RenderScene(Scene* scene)
{
    if (!scene)
        return;

    const float aspect = (m_height > 0) ? (static_cast<float>(m_width) / static_cast<float>(m_height)) : (16.0f / 9.0f);
    FrameCameraData frameData;
    frameData.viewMatrix = BuildLookAtRH(m_cameraEye, m_cameraTarget, vec3(0.0f, 1.0f, 0.0f));
    frameData.projMatrix = BuildPerspectiveRH(1.0471976f, aspect, 0.1f, 100.0f);
    frameData.cameraPosition = vec4(m_cameraEye.x, m_cameraEye.y, m_cameraEye.z, 1.0f);

    for (auto& entity : scene->GetEntities())
    {
        if (!entity)
            continue;

        if (entity->isBillboardActor)
        {
            const mat4 billboardWorld = BuildBillboardWorld(entity->transform.position, entity->transform.scale, m_cameraEye);
            entity->Draw(m_commandList.Get(), frameData, &billboardWorld, nullptr, false);
            continue;
        }

        entity->Draw(m_commandList.Get(), frameData);
    }

    // Projected soft shadows onto ground plane (index 0 entity).
    // Keep projected shadows slightly above the floor to avoid depth z-fighting.
    const float groundPlaneY = -0.985f;
    const vec3 shadowRayDir = Vec3Normalize(vec3(-0.40f, -0.95f, -0.55f));
    const mat4 shadowProj = BuildDirectionalShadowProjection(groundPlaneY, shadowRayDir);

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
        Entity* entity = scene->GetEntities()[i].get();
        if (!entity || !entity->castsProjectedShadow)
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
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    if (FAILED(m_commandList->Close()))
        return;

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    m_swapChain->Present(1, 0);
    MoveToNextFrame();
}
