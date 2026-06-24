#pragma once

#pragma warning(disable: 4530 4577)  // Disable cmath-related errors

#include <d3d12.h>
#include <wrl.h>
#include "EngineMath.h"

class Material
{
public:
    enum class InitFailureStage
    {
        None = 0,
        RootSignature,
        TextureResources,
        PipelineState,
    };

    Material() = default;
    ~Material();

    bool Init(ID3D12Device* device, ID3D12CommandQueue* commandQueue, const wchar_t* texturePath = nullptr);
    void Shutdown();

    ID3D12PipelineState* GetPipelineState() const { return m_pipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }
    ID3D12DescriptorHeap* GetSrvHeap() const { return m_srvHeap.Get(); }
    InitFailureStage GetLastInitFailureStage() const { return m_lastInitFailureStage; }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;
    InitFailureStage m_lastInitFailureStage = InitFailureStage::None;

    bool CreateRootSignature(ID3D12Device* device);
    bool CreatePipelineState(ID3D12Device* device);
    bool CreateTextureResources(ID3D12Device* device, ID3D12CommandQueue* commandQueue, const wchar_t* texturePath);
};
