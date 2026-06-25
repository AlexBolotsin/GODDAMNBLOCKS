#include "Material.h"
#include <d3dcompiler.h>
#include <cstring>
#include <wincodec.h>
#include <vector>

namespace
{
    void ReportMaterialInitFailure(const char* message)
    {
        OutputDebugStringA(message);
        OutputDebugStringA("\n");
    }

    struct LoadedTextureData
    {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> pixels;
    };

    bool LoadTexturePixelsWIC(const wchar_t* texturePath, LoadedTextureData& outTexture)
    {
        if (!texturePath)
        {
            outTexture.width = 1;
            outTexture.height = 1;
            outTexture.pixels = { 255, 255, 255, 255 };
            return true;
        }

        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;

        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
        if (FAILED(hr))
        {
            return false;
        }

        hr = factory->CreateDecoderFromFilename(texturePath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr))
        {
            OutputDebugStringW(L"LoadTexturePixelsWIC: CreateDecoderFromFilename failed\n");
            return false;
        }

        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr))
        {
            return false;
        }

        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr))
        {
            return false;
        }

        hr = converter->Initialize(
            frame.Get(),
            GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr))
        {
            return false;
        }

        UINT width = 0;
        UINT height = 0;
        hr = converter->GetSize(&width, &height);
        if (FAILED(hr) || width == 0 || height == 0)
        {
            return false;
        }

        outTexture.width = width;
        outTexture.height = height;
        outTexture.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4ull);

        hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(outTexture.pixels.size()), outTexture.pixels.data());

        return SUCCEEDED(hr);
    }
}

Material::~Material()
{
    Shutdown();
}

bool Material::Init(ID3D12Device* device, ID3D12CommandQueue* commandQueue, const wchar_t* texturePath, uint32_t sampleCount)
{
    m_lastInitFailureStage = InitFailureStage::None;
    m_sampleCount = sampleCount;

    if (!CreateRootSignature(device))
    {
        m_lastInitFailureStage = InitFailureStage::RootSignature;
        ReportMaterialInitFailure("Material::Init failed in CreateRootSignature");
        return false;
    }

    if (!CreateTextureResources(device, commandQueue, texturePath))
    {
        m_lastInitFailureStage = InitFailureStage::TextureResources;
        ReportMaterialInitFailure("Material::Init failed in CreateTextureResources");
        return false;
    }

    if (!CreatePipelineState(device))
    {
        m_lastInitFailureStage = InitFailureStage::PipelineState;
        ReportMaterialInitFailure("Material::Init failed in CreatePipelineState");
        return false;
    }
    
    return true;
}

void Material::Shutdown()
{
    if (m_pipelineState)
        m_pipelineState.Reset();
    if (m_rootSignature)
        m_rootSignature.Reset();
    if (m_srvHeap)
        m_srvHeap.Reset();
    if (m_texture)
        m_texture.Reset();
}

bool Material::CreateRootSignature(ID3D12Device* device)
{
    D3D12_ROOT_PARAMETER rootParams[3] = {};
    D3D12_DESCRIPTOR_RANGE descriptorRanges[1] = {};

    D3D12_ROOT_CONSTANTS perFrameConstants = {};
    perFrameConstants.Num32BitValues = 16 + 16; // view + projection
    perFrameConstants.ShaderRegister = 0;
    perFrameConstants.RegisterSpace = 0;

    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants = perFrameConstants;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_CONSTANTS perObjectConstants = {};
    perObjectConstants.Num32BitValues = 16 + 4 + 4 + 4; // world + tint + render params + sprite uv rect
    perObjectConstants.ShaderRegister = 1;
    perObjectConstants.RegisterSpace = 0;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[1].Constants = perObjectConstants;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].RegisterSpace = 0;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = descriptorRanges;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.MipLODBias = 0.0f;
    staticSampler.MaxAnisotropy = 1;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    staticSampler.MinLOD = 0.0f;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.RegisterSpace = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 3;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &staticSampler;
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
    {
        ReportMaterialInitFailure("CreateRootSignature: device->CreateRootSignature failed");
        return false;
    }

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
};

cbuffer PerDraw : register(b1)
{
    row_major float4x4 worldMatrix;
    float4 tintColor;
    float4 renderParams;
    float4 spriteUVRect;
};

Texture2D spriteTexture : register(t0);
SamplerState spriteSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float3 worldPos : TEXCOORD0;
    float3 normalWS : TEXCOORD1;
    float2 clipYW   : TEXCOORD2;
    float2 uv       : TEXCOORD3;
    float3 viewPos  : TEXCOORD4;
};

float SampleSpriteAlpha(in float2 uv, out float3 color)
{
    float4 texel = spriteTexture.Sample(spriteSampler, uv);
    float3 chromaKey = float3(34.0f / 255.0f, 177.0f / 255.0f, 76.0f / 255.0f);
    float chromaDistance = distance(texel.rgb, chromaKey);
    float alpha = texel.a * smoothstep(0.10f, 0.16f, chromaDistance);
    color = texel.rgb;
    return alpha;
}

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    output.position = mul(viewPos, projMatrix);
    output.color = tintColor;
    output.worldPos = worldPos.xyz;
    output.clipYW = float2(output.position.y, output.position.w);
    output.uv = input.uv;
    output.viewPos = viewPos.xyz;

    output.normalWS = normalize(mul(float4(input.normal, 0.0f), worldMatrix).xyz);

    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    if (renderParams.x > 0.5f)
    {
        if (renderParams.y > 0.5f)
        {
            float2 spriteUv = lerp(spriteUVRect.xy, spriteUVRect.zw, input.uv);
            float3 spriteColor;
            float spriteAlpha = SampleSpriteAlpha(spriteUv, spriteColor);
            clip(spriteAlpha - 0.05f);
        }

        float cameraDistance = length(input.viewPos);
        float fogStart = 8.0f;
        float fogEnd = 22.0f;
        float fogFactor = saturate((cameraDistance - fogStart) / (fogEnd - fogStart));

        float alpha = input.color.a * (1.0f - fogFactor * 0.5f);
        return float4(0.0f, 0.0f, 0.0f, alpha);
    }

    if (renderParams.y > 0.5f)
    {
        float2 spriteUv = lerp(spriteUVRect.xy, spriteUVRect.zw, input.uv);
        float3 sampledSpriteColor;
        float spriteAlpha = SampleSpriteAlpha(spriteUv, sampledSpriteColor);
        clip(spriteAlpha - 0.05f);

        float cameraDistance = length(input.viewPos);
        float fogStart = 8.0f;
        float fogEnd = 22.0f;
        float fogFactor = saturate((cameraDistance - fogStart) / (fogEnd - fogStart));

        float ndcY = input.clipYW.x / max(abs(input.clipYW.y), 1e-4f);
        float skyT = saturate(ndcY * 0.5f + 0.5f);
        float3 skyHorizon = float3(0.64f, 0.70f, 0.78f);
        float3 skyZenith = float3(0.24f, 0.38f, 0.62f);
        float3 fogColor = lerp(skyHorizon, skyZenith, skyT);

        float3 spriteColor = sampledSpriteColor * input.color.rgb;
        float3 finalSpriteColor = lerp(spriteColor, fogColor, fogFactor * 0.35f);
        return float4(saturate(finalSpriteColor), spriteAlpha * input.color.a);
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

    float3 viewDir = normalize(-input.viewPos);

    float3 ambientColor = float3(0.20f, 0.21f, 0.23f);
    float keyDiffuse = saturate(dot(normalWS, keyToLight));
    float fillDiffuse = saturate(dot(normalWS, fillToLight));

    float keyBand = keyDiffuse;
    float fillBand = fillDiffuse;
    float ndotv = saturate(dot(normalWS, viewDir));
    float edgeBand = smoothstep(0.62f, 0.90f, 1.0f - ndotv);

    // Keep a tiny floor for readability so fully unlit areas are not crushed to black.
    float3 diffuseLighting = keyColor * (0.10f + keyBand * 0.75f) + fillColor * (fillBand * 0.30f);
    float3 litColor = baseColor * (ambientColor + diffuseLighting);
    litColor *= (1.0f - edgeBand * 0.14f);
    litColor = min(litColor, float3(1.0f, 1.0f, 1.0f));

    float cameraDistance = length(input.viewPos);
    float fogStart = 8.0f;
    float fogEnd = 22.0f;
    float fogFactor = saturate((cameraDistance - fogStart) / (fogEnd - fogStart));

    float ndcY = input.clipYW.x / max(abs(input.clipYW.y), 1e-4f);
    float skyT = saturate(ndcY * 0.5f + 0.5f);

    float3 skyHorizon = float3(0.64f, 0.70f, 0.78f);
    float3 skyZenith = float3(0.24f, 0.38f, 0.62f);
    float3 fogColor = lerp(skyHorizon, skyZenith, skyT);

    float3 finalColor = saturate(lerp(litColor, fogColor, fogFactor));
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
        ReportMaterialInitFailure("CreatePipelineState: VS compile failed");
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
        ReportMaterialInitFailure("CreatePipelineState: PS compile failed");
        return false;
    }

    static const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
    rasterizerDesc.MultisampleEnable = TRUE;
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
    psoDesc.SampleDesc.Count = m_sampleCount;
    psoDesc.SampleDesc.Quality = 0;

    if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState))))
    {
        ReportMaterialInitFailure("CreatePipelineState: device->CreateGraphicsPipelineState failed");
        return false;
    }

    return true;
}

bool Material::CreateTextureResources(ID3D12Device* device, ID3D12CommandQueue* commandQueue, const wchar_t* texturePath)
{
    if (!device || !commandQueue)
    {
        ReportMaterialInitFailure("CreateTextureResources: device or commandQueue was null");
        return false;
    }

    LoadedTextureData textureData;
    if (!LoadTexturePixelsWIC(texturePath, textureData))
    {
        ReportMaterialInitFailure("CreateTextureResources: LoadTexturePixelsWIC failed");
        return false;
    }

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = 0;
    textureDesc.Width = textureData.width;
    textureDesc.Height = textureData.height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_texture))))
    {
        return false;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &uploadSize);

    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    if (FAILED(device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer))))
    {
        return false;
    }

    uint8_t* mappedData = nullptr;
    if (FAILED(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData))))
        return false;

    const size_t sourceRowPitch = static_cast<size_t>(textureData.width) * 4ull;
    for (UINT row = 0; row < numRows; ++row)
    {
        memcpy(
            mappedData + footprint.Offset + static_cast<size_t>(row) * footprint.Footprint.RowPitch,
            textureData.pixels.data() + static_cast<size_t>(row) * sourceRowPitch,
            sourceRowPitch);
    }
    uploadBuffer->Unmap(0, nullptr);

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))))
        return false;
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList))))
        return false;

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = m_texture.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = uploadBuffer.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = footprint;

    commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    commandList->ResourceBarrier(1, &barrier);

    if (FAILED(commandList->Close()))
        return false;

    ID3D12CommandList* commandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, commandLists);

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
        return false;

    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent)
        return false;

    const UINT64 fenceValue = 1;
    if (FAILED(commandQueue->Signal(fence.Get(), fenceValue)))
    {
        CloseHandle(fenceEvent);
        return false;
    }

    if (fence->GetCompletedValue() < fenceValue)
    {
        if (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent)))
        {
            CloseHandle(fenceEvent);
            return false;
        }

        WaitForSingleObject(fenceEvent, INFINITE);
    }

    CloseHandle(fenceEvent);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap))))
        return false;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}
