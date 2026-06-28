#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <utility>
#include <vector>
#include "EngineMath.h"
#include "Camera.h"
#include "ECS.h"

class World;

class DX12Context
{
public:
    DX12Context() = default;
    ~DX12Context();

    bool Init(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);

    // Per-frame camera + lighting data passed from RenderScene to EndFrame via member
    struct FrameCameraData
    {
        mat4 viewMatrix;
        mat4 projMatrix;
        mat4 lightViewProjMatrix;
        D3D12_GPU_VIRTUAL_ADDRESS perFrameGpuAddr = 0;
    };

    void BeginFrame();
    void RenderScene(World* world, const Camera& camera);
    void EndFrame();

    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    uint32_t GetMsaaSampleCount() const { return m_msaaSampleCount; }
    ID3D12Resource* GetShadowMap() const { return m_shadowMap.Get(); }
    void SetFPS(float fps)           { m_fps = fps; }
    void SetScanlinesEnabled(bool v) { m_scanlinesEnabled = v; }
    void SetDitherEnabled(bool v)    { m_ditherEnabled = v; }

private:
    void LogAllAdapters();
    void LogAdapterInfo(Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter);

    bool CreateDevice();
    bool CreateCommandObjects();
    bool CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
    bool CreateRenderTargetViews();
    bool CreateDSVHeap();
    bool CreateDepthStencilBuffer();
    bool CreateFenceAndEvent();
    bool CreateShadowMapResources();
    bool CreateShadowPipeline();
    bool CreateShadowSpritePipeline();
    bool CreateInstancedSpritePipeline();
    bool CreateParticlePipeline();
    bool CreatePostProcessResources();
    bool CreatePostProcessPipelines();
    bool CreateDistortPipeline();
    void WaitForGpu();
    void MoveToNextFrame();

private:
    static constexpr uint32_t FrameCount = 2;

    static constexpr uint32_t kShadowMapSize = 2048;

    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_msaaColorTarget;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;

    Microsoft::WRL::ComPtr<ID3D12Resource>       m_shadowMap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_shadowDsvHeap;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_shadowPso;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_shadowRootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_shadowSpritePso;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_shadowSpriteRootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_instancedSpritePso;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_instancedSpriteRootSig;
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_spriteInstanceBuffer;
    uint8_t*                                      m_spriteInstanceMapped = nullptr;
    static constexpr uint32_t kMaxSpriteInstances = 5000;

    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_particlePso;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_particleRootSig;
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_particleBuffer;
    uint8_t*                                      m_particleMapped = nullptr;
    static constexpr uint32_t kMaxParticles = 8000;
    std::vector<std::pair<float, EntityID>> m_instanceSortBuf;
    mat4  m_lightViewProj;
    vec3  m_lightPos;
    float    m_fps              = 0.0f;
    bool     m_scanlinesEnabled = false;
    bool     m_ditherEnabled    = false;
    uint32_t m_entityCount      = 0;
    uint32_t m_drawCallCount    = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_perFrameCb;
    uint8_t* m_perFrameCbMapped = nullptr;

    // Post-process / HDR / bloom
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_hdrTarget;      // R16G16B16A16_FLOAT, full-res
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_bloomA;         // R16G16B16A16_FLOAT, half-res
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_bloomB;         // R16G16B16A16_FLOAT, half-res
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_distortTarget;  // R16G16B16A16_FLOAT, full-res
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_postRtvHeap;    // 4 RTVs: hdr, bloomA, bloomB, distort
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_postSrvHeap;    // 5 SRVs (shader-visible)
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_postRootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_brightPassPso;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_blurHPso;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_blurVPso;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_tonemapPso;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_distortRootSig;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  m_distortPso;
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_shockwaveCb[FrameCount];
    uint8_t*                                     m_shockwaveCbMapped[FrameCount] = {};
    static constexpr uint32_t                    kMaxShockwaves = 8;
    uint32_t m_bloomWidth  = 0;
    uint32_t m_bloomHeight = 0;

    HANDLE m_fenceEvent = nullptr;
    uint64_t m_fenceValues[FrameCount] = {};
    uint32_t m_rtvDescriptorSize = 0;
    DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D32_FLOAT;
    DXGI_FORMAT m_msaaColorFormat = DXGI_FORMAT_R32_TYPELESS;
    uint32_t m_msaaSampleCount = 8;
    uint32_t m_msaaQuality = 0;
    uint32_t m_frameIndex = 0;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
