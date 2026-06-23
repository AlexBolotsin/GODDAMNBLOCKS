#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>

class Scene;

class DX12Context
{
public:
    DX12Context() = default;
    ~DX12Context();

    bool Init(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);
    
    void BeginFrame();
    void RenderScene(Scene* scene);
    void EndFrame();

    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }

private:
    bool CreateDevice();
    bool CreateCommandObjects();
    bool CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
    bool CreateRenderTargetViews();
    bool CreateFenceAndEvent();
    void WaitForGpu();
    void MoveToNextFrame();

private:
    static constexpr uint32_t FrameCount = 2;

    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;

    HANDLE m_fenceEvent = nullptr;
    uint64_t m_fenceValues[FrameCount] = {};
    uint32_t m_rtvDescriptorSize = 0;
    uint32_t m_frameIndex = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
