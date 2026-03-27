#include "GBufferNormalMapping.hlsl"

cbuffer GeometryCB : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;
    float4 albedo;
    float4 tessParams;
};

Texture2D normalMapTex : register(t1);
Texture2D displacementTex : register(t2);
SamplerState baseColorSampler : register(s0);

struct VSOutput
{
    float4 posH : SV_POSITION;
    float3 posV : TEXCOORD0;
    float3 normalV : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 posW : TEXCOORD3;
    float3 normalW : TEXCOORD4;
};

struct HSConstantData
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

[domain("tri")]
VSOutput DSMain(HSConstantData patch_data, const OutputPatch<VSOutput, 3> patch, float3 bary : SV_DomainLocation)
{
    float u = bary.x;
    float v = bary.y;
    float w = bary.z;

    float3 interpolated_posW = patch[0].posW * u + patch[1].posW * v + patch[2].posW * w;
    float3 interpolated_normalW = patch[0].normalW * u + patch[1].normalW * v + patch[2].normalW * w;
    float2 interpolated_uv = patch[0].uv * u + patch[1].uv * v + patch[2].uv * w;

    float3 normalW = normalize(interpolated_normalW);

    float3 p0 = patch[0].posW;
    float3 p1 = patch[1].posW;
    float3 p2 = patch[2].posW;
    float2 uv0 = patch[0].uv;
    float2 uv1 = patch[1].uv;
    float2 uv2 = patch[2].uv;
    float3 T, B;
    BuildTriangleTBN(p0, p1, p2, uv0, uv1, uv2, normalW, T, B);

    float4 nmSample = normalMapTex.SampleLevel(baseColorSampler, interpolated_uv, 0);
    bool hasNM = !(abs(nmSample.r - 1.0f) < 0.01f && abs(nmSample.g - 1.0f) < 0.01f && abs(nmSample.b - 1.0f) < 0.01f);
    float3 nTS = float3(0.0f, 0.0f, 1.0f);
    float3 bumpW = normalW;
    if (hasNM)
    {
        nTS = DecodeNormalMapSample(nmSample);
        bumpW = NormalFromTsWithTBN(nTS, normalW, T, B);
    }
    float tiltNM = hasNM ? saturate(1.0f - nTS.z) : 0.0f;
    float lateralNM = hasNM ? length(nTS.xy) : 0.0f;

    // Height from optional displacement map (neutral ~0.5)
    float4 displacementSample = displacementTex.SampleLevel(baseColorSampler, interpolated_uv, 0);
    bool hasDisplacement = !(abs(displacementSample.r - 1.0f) < 0.01f && abs(displacementSample.g - 1.0f) < 0.01f && abs(displacementSample.b - 1.0f) < 0.01f);
    float hDisp = 0.0f;
    if (hasDisplacement) {
        hDisp = (displacementSample.r - 0.5f) * tessParams.z;
    }

    // Height from normal map — gentle curve avoids needle-like extrusions on tight edge gradients
    float hNm = 0.0f;
    if (hasNM && tessParams.w > 0.0f) {
        float edgeAmt = saturate(tiltNM * 0.45f + lateralNM * 0.28f);
        hNm = tessParams.w * edgeAmt * edgeAmt;
    }

    float h = hDisp + hNm;

    float3 dirW = normalW;
    if (hasNM && abs(h) > 1e-6f) {
        float edgeBlend = saturate(pow(max(lateralNM, tiltNM), 1.15f)) * 0.45f;
        dirW = normalize(lerp(normalW, bumpW, edgeBlend));
    }

    interpolated_posW += dirW * h;

    float4 posV = mul(float4(interpolated_posW, 1.0f), view);

    float3 normalV = mul(float4(normalW, 0.0f), view).xyz;

    VSOutput result;
    result.posW = interpolated_posW;
    result.posV = posV.xyz;
    result.normalW = normalW;
    result.normalV = normalV;
    result.uv = interpolated_uv;
    result.posH = mul(posV, proj);

    return result;
}
