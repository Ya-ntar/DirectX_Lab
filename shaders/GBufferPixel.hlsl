#include "GBufferNormalMapping.hlsl"

cbuffer GeometryCB : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;
    float4 albedo;
    float4 tessParams;
};

Texture2D baseColorTex : register(t0);
Texture2D normalMapTex : register(t1);
SamplerState baseColorSampler : register(s0);

struct PSInput
{
    float4 posH : SV_POSITION;
    float3 posV : TEXCOORD0;       // Position in view-space
    float3 normalV : TEXCOORD1;    // Normal in view-space
    float2 uv : TEXCOORD2;
    float3 posW : TEXCOORD3;       // World-space position (unused but passed through)
    float3 normalW : TEXCOORD4;    // World-space normal (unused but passed through)
};

struct PSOutput
{
    float4 posV : SV_TARGET0;     // Store view-space position for lighting calculations
    float4 normalV : SV_TARGET1;  // Store view-space normal for lighting calculations
    float4 albedoOut : SV_TARGET2; // Store base color (independent of lighting)
};

PSOutput PSMain(PSInput input)
{
    PSOutput o;
    float4 tex = baseColorTex.Sample(baseColorSampler, input.uv);

    float4 normalMapSample = normalMapTex.Sample(baseColorSampler, input.uv);
    bool hasNormalMap = !(abs(normalMapSample.r - 1.0f) < 0.01f && abs(normalMapSample.g - 1.0f) < 0.01f && abs(normalMapSample.b - 1.0f) < 0.01f);

    float3 Nw = normalize(input.normalW);
    float3 bumpW;
    if (hasNormalMap)
    {
        float3 nTS = DecodeNormalMapSample(normalMapSample);
        bumpW = NormalFromTsToWorld(Nw, input.posW, input.uv, nTS);
    }
    else
    {
        bumpW = Nw;
    }

    float3 normalV = normalize(mul(float4(bumpW, 0.0f), view).xyz);

    o.posV = float4(input.posV, 1.0f);
    o.normalV = float4(normalV, 1.0f);
    o.albedoOut = float4(tex.rgb * albedo.rgb, 1.0f);
    return o;
}
