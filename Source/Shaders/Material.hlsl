cbuffer PerObject : register(b0)
{
    row_major float4x4 viewMatrix;
    row_major float4x4 projMatrix;
    row_major float4x4 lightViewProjMatrix;
    float3 cameraEyeWS;
    float  _pad;
};

cbuffer PerDraw : register(b1)
{
    row_major float4x4 worldMatrix;
    float4 tintColor;
    float4 renderParams;
    float4 spriteUVRect;
};

Texture2D            spriteTexture : register(t0);
Texture2D<float>     shadowMap     : register(t1);
SamplerState               spriteSampler : register(s0);
SamplerComparisonState     shadowSampler : register(s1);

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

float SampleShadow(float3 worldPos)
{
    float4 ls = mul(float4(worldPos, 1.0f), lightViewProjMatrix);
    float2 uv = float2(ls.x * 0.5f + 0.5f, ls.y * -0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f))
        return 1.0f;

    float depth = ls.z - 0.005f;
    float shadow = 0.0f;
    float2 texelSize = 1.0f / 2048.0f;
    [unroll] for (int dx = -1; dx <= 1; ++dx)
        [unroll] for (int dy = -1; dy <= 1; ++dy)
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, uv + float2(dx, dy) * texelSize, depth);
    return shadow / 9.0f;
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

    float3 keyToLight = normalize(float3(0.40f, 0.95f, 0.55f));
    float3 keyColor = float3(1.00f, 0.96f, 0.90f);

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

    float shadowFactor = SampleShadow(input.worldPos);
    float3 diffuseLighting = keyColor * (0.10f + keyBand * 1.00f * shadowFactor) + fillColor * (fillBand * 0.30f);
    float3 litColor = baseColor * (ambientColor + diffuseLighting);
    litColor *= (1.0f - edgeBand * 0.14f);

    // Blinn-Phong specular — high peak so bright faces exceed 1.0 and bloom
    float3 viewDirWS = normalize(cameraEyeWS - input.worldPos);
    float3 halfVec   = normalize(keyToLight + viewDirWS);
    float  NdotH     = saturate(dot(normalWS, halfVec));
    float  spec      = pow(NdotH, 64.0f) * 2.50f * (1.0f - floorMask * 0.85f) * shadowFactor;
    litColor        += keyColor * spec;
    // No clamp — HDR values > 1.0 feed the bloom bright-pass

    float cameraDistance = length(input.viewPos);
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
