#include "DX12Context.h"
#include "Scene.h"


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

    if (FAILED(m_swapChain->ResizeBuffers(FrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0)))
        return;

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    CreateRenderTargetViews();
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

    const FLOAT clearColor[4] = { 0.12f, 0.18f, 0.27f, 1.0f };
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

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

    for (auto& entity : scene->GetEntities())
    {
        entity->Draw(m_commandList.Get());
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
