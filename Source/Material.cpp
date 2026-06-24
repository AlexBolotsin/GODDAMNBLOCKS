#include "Material.h"
#include <d3dcompiler.h>
#include <cstring>

Material::~Material()
{
    Shutdown();
}

bool Material::Init(ID3D12Device* device)
{
    if (!CreateRootSignature(device))
        return false;

    if (!CreatePipelineState(device))
        return false;
    
    return true;
}

void Material::Shutdown()
{
    if (m_pipelineState)
        m_pipelineState.Reset();
    if (m_rootSignature)
        m_rootSignature.Reset();
}

bool Material::CreateRootSignature(ID3D12Device* device)
{
    D3D12_ROOT_PARAMETER rootParams[2] = {};

    D3D12_ROOT_CONSTANTS perFrameConstants = {};
    perFrameConstants.Num32BitValues = 16 + 16 + 4; // view + projection + camera position
    perFrameConstants.ShaderRegister = 0;
    perFrameConstants.RegisterSpace = 0;

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants = perFrameConstants;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_CONSTANTS perObjectConstants = {};
    perObjectConstants.Num32BitValues = 16 + 4 + 4; // world + tint + render params
    perObjectConstants.ShaderRegister = 1;
    perObjectConstants.RegisterSpace = 0;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[1].Constants = perObjectConstants;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 2;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

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
    if (!device || !m_rootSignature)
        return false;

    static const char* kShaderSource = R"(
cbuffer PerObject : register(b0)
{
    row_major float4x4 viewMatrix;
    row_major float4x4 projMatrix;
    float4 cameraPosition;
};

cbuffer PerDraw : register(b1)
{
    row_major float4x4 worldMatrix;
    float4 tintColor;
    float4 renderParams;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float3 worldPos : TEXCOORD0;
    float3 normalWS : TEXCOORD1;
    float2 clipYW   : TEXCOORD2;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.position = mul(viewPos, projMatrix);
    output.color = tintColor;
    output.worldPos = worldPos.xyz;
    output.clipYW = float2(output.position.y, output.position.w);

    output.normalWS = normalize(mul(float4(input.normal, 0.0f), worldMatrix).xyz);

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    if (renderParams.x > 0.5f)
    {
        float cameraDistance = distance(input.worldPos, cameraPosition.xyz);
        float fogStart = 8.0f;
        float fogEnd = 22.0f;
        float fogFactor = saturate((cameraDistance - fogStart) / (fogEnd - fogStart));

        float alpha = input.color.a * (1.0f - fogFactor * 0.5f);
        return float4(0.0f, 0.0f, 0.0f, alpha);
    }

    float3 normalWS = normalize(input.normalWS);

    // Apply floor-only patterning and micro normal variation around the ground plane (y ~= -1).
    float floorMask = saturate(1.0f - abs(input.worldPos.y + 1.0f) * 4.0f);

    float2 floorUV = input.worldPos.xz * 0.75f;
    float checker = fmod(floor(floorUV.x) + floor(floorUV.y), 2.0f);
    float2 cell = frac(floorUV);
    float2 edgeDist = min(cell, 1.0f - cell);
    float edge = min(edgeDist.x, edgeDist.y);
    float gridLine = 1.0f - smoothstep(0.0f, 0.03f, edge);

    float3 floorA = float3(0.20f, 0.23f, 0.26f);
    float3 floorB = float3(0.27f, 0.30f, 0.34f);
    float3 floorPattern = lerp(floorA, floorB, checker);
    float variation = sin(input.worldPos.x * 0.35f) * cos(input.worldPos.z * 0.41f) * 0.04f;
    floorPattern += variation;
    floorPattern = lerp(floorPattern, floorPattern * 0.55f, gridLine * 0.70f);

    float2 wave = float2(
        sin(input.worldPos.x * 0.70f + input.worldPos.z * 0.20f),
        cos(input.worldPos.z * 0.65f - input.worldPos.x * 0.15f));
    float3 floorNormal = normalize(normalWS + float3(wave.x, 0.0f, wave.y) * (0.18f * floorMask));
    normalWS = normalize(lerp(normalWS, floorNormal, floorMask));

    float3 baseColor = lerp(input.color.rgb, floorPattern * input.color.rgb, floorMask);

    // Key light (sun): warm and directional.
    float3 keyToLight = normalize(float3(0.40f, 0.95f, 0.55f));
    float3 keyColor = float3(1.00f, 0.96f, 0.90f);

    // Fill light: soft, cool, and opposite-ish direction for shape readability.
    float3 fillToLight = normalize(float3(-0.45f, 0.70f, -0.55f));
    float3 fillColor = float3(0.42f, 0.54f, 0.78f);

    float3 viewDir = normalize(cameraPosition.xyz - input.worldPos);
    float3 keyHalfVec = normalize(keyToLight + viewDir);

    float3 ambientColor = float3(0.20f, 0.21f, 0.23f);
    float keyDiffuse = saturate(dot(normalWS, keyToLight));
    float fillDiffuse = saturate(dot(normalWS, fillToLight));

    float keySpecular = pow(saturate(dot(normalWS, keyHalfVec)), 32.0f) * 0.35f;

    float rim = pow(1.0f - saturate(dot(normalWS, viewDir)), 3.0f);
    rim *= 0.25f;
    float3 rimColor = float3(0.55f, 0.65f, 0.90f);

    float3 diffuseLighting = keyColor * (keyDiffuse * 0.85f) + fillColor * (fillDiffuse * 0.40f);
    float3 litColor = baseColor * (ambientColor + diffuseLighting);
    litColor += float3(keySpecular, keySpecular, keySpecular);
    litColor += rimColor * rim;

    float cameraDistance = distance(input.worldPos, cameraPosition.xyz);
    float fogStart = 8.0f;
    float fogEnd = 22.0f;
    float fogFactor = saturate((cameraDistance - fogStart) / (fogEnd - fogStart));

    float ndcY = input.clipYW.x / max(abs(input.clipYW.y), 1e-4f);
    float skyT = saturate(ndcY * 0.5f + 0.5f);

    float3 skyHorizon = float3(0.64f, 0.70f, 0.78f);
    float3 skyZenith = float3(0.24f, 0.38f, 0.62f);
    float3 fogColor = lerp(skyHorizon, skyZenith, skyT);

    float3 finalColor = lerp(litColor, fogColor, fogFactor);
    return float4(finalColor, input.color.a);
}
)";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        kShaderSource,
        std::strlen(kShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        compileFlags,
        0,
        &vsBlob,
        &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    errorBlob.Reset();
    hr = D3DCompile(
        kShaderSource,
        std::strlen(kShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        compileFlags,
        0,
        &psBlob,
        &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    static const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc = {};
    rtBlendDesc.BlendEnable = TRUE;
    rtBlendDesc.LogicOpEnable = FALSE;
    rtBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rtBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    rtBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    rtBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rtBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    rtBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    for (int i = 0; i < 8; ++i)
        blendDesc.RenderTarget[i] = rtBlendDesc;

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FrontCounterClockwise = TRUE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
    psoDesc.VS.BytecodeLength = vsBlob->GetBufferSize();
    psoDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
    psoDesc.PS.BytecodeLength = psBlob->GetBufferSize();
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.InputLayout.pInputElementDescs = inputLayout;
    psoDesc.InputLayout.NumElements = static_cast<UINT>(_countof(inputLayout));
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState))))
        return false;

    return true;
}
