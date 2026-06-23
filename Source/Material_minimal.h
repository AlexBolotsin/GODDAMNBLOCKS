#pragma once

// Minimal Material implementation without runtime shader compilation
// This uses placeholder shader bytecode - normally would be pre-compiled

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

using namespace Microsoft::WRL;

class Material
{
public:
    Material() = default;
    ~Material();

    bool Init(ID3D12Device* device);
    void Shutdown();
    
    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return m_pipelineState.Get(); }

    vec4 color { 1, 1, 1, 1 };

private:
    bool CreateRootSignature(ID3D12Device* device);
    bool CreatePipelineState(ID3D12Device* device);
    
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
};
