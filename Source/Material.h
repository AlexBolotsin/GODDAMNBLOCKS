#pragma once

#pragma warning(disable: 4530 4577)  // Disable cmath-related errors

#include <d3d12.h>
#include <wrl.h>
#include "EngineMath.h"

class Material
{
public:
    Material() = default;
    ~Material();

    bool Init(ID3D12Device* device);
    void Shutdown();

    ID3D12PipelineState* GetPipelineState() const { return m_pipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

    vec4 color = vec4(1.0f, 1.0f, 1.0f, 1.0f);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

    bool CreateRootSignature(ID3D12Device* device);
    bool CreatePipelineState(ID3D12Device* device);
};
