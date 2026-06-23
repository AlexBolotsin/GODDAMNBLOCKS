#include "Material.h"
#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

// Dummy shader bytecode (empty for now - would normally be pre-compiled)
static const uint8_t g_VertexShaderBytecode[] = {
    // This would contain actual compiled HLSL bytecode
    // For now, using placeholder - rendering will not work correctly
};

static const uint8_t g_PixelShaderBytecode[] = {
    // This would contain actual compiled HLSL bytecode  
    // For now, using placeholder - rendering will not work correctly
};

Material::~Material()
{
    Shutdown();
}

bool Material::Init(ID3D12Device* device)
{
    if (!CreateRootSignature(device))
        return false;

    // Skip pipeline state creation for now to avoid d3dcompiler.h dependency
    // This is a temporary workaround for MSVC cmath header conflicts

    return true;
}

void Material::Shutdown()
{
    m_pipelineState.Reset();
    m_rootSignature.Reset();
}

bool Material::CreateRootSignature(ID3D12Device* device)
{
    D3D12_ROOT_PARAMETER rootParams[1] = {};
    
    D3D12_ROOT_CONSTANTS rootConstants = {};
    rootConstants.Num32BitValues = 16 + 4; // mat4 + color vec4
    rootConstants.ShaderRegister = 0;
    rootConstants.RegisterSpace = 0;
    
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants = rootConstants;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;

    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob)))
    {
        if (errorBlob)
        {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return false;
    }

    if (FAILED(device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature))))
        return false;

    return true;
}

bool Material::CreatePipelineState(ID3D12Device* device)
{
    // Temporarily disabled due to MSVC cmath header conflicts with d3dcompiler.h
    // This would require either:
    // 1. Pre-compiled shader bytecode (offline compilation with FXC/DXC)
    // 2. Using a different compiler (Clang, MinGW)
    // 3. Fixing the MSVC/Windows SDK environment
    
    return false; // Pipeline state creation disabled
}
