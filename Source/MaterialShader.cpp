#pragma warning(push)
#pragma warning(disable: 4530 4577 C3861 C2039 C2182)

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <cstring>

#pragma warning(pop)

#include "Material.h"

bool Material::CreatePipelineState(ID3D12Device* device)
{
    const char* hlslCode = R"(
        cbuffer PerObject : register(b0)
        {
            float4x4 worldMatrix;
            float4 color;
        };

        struct VS_INPUT
        {
            float3 position : POSITION;
            float3 normal : NORMAL;
            float4 color : COLOR;
        };

        struct PS_INPUT
        {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        PS_INPUT main(VS_INPUT input)
        {
            PS_INPUT output;
            float4 worldPos = mul(float4(input.position, 1.0f), worldMatrix);
            output.position = worldPos;
            output.color = color;
            return output;
        }
    )";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

    // Compile vertex shader
    if (FAILED(D3DCompile(hlslCode, strlen(hlslCode), nullptr, nullptr, nullptr,
                          "main", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vsBlob, &errorBlob)))
    {
        if (errorBlob)
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        return false;
    }

    // Compile pixel shader
    if (FAILED(D3DCompile(hlslCode, strlen(hlslCode), nullptr, nullptr, nullptr,
                          "main", "ps_5_0", D3DCOMPILE_DEBUG, 0, &psBlob, &errorBlob)))
    {
        if (errorBlob)
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
    psoDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
    psoDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
    psoDesc.PS.BytecodeLength = psBlob->GetBufferSize();
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.SampleMask = 0xFFFFFFFF;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState))))
        return false;

    return true;
}
