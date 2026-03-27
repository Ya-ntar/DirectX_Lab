cbuffer GeometryCB : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;
    float4 albedo;
    float4 tessParams;
};

struct VSOutput
{
    float4 posH : SV_POSITION;
    float3 posV : TEXCOORD0;
    float3 normalV : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

struct HSConstantData
{
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

struct PSOutput
{
    float4 posV : SV_TARGET0;
    float4 normalV : SV_TARGET1;
    float4 albedoOut : SV_TARGET2;
};

[domain("quad")]
VSOutput DSMain(HSConstantData patch_data, const OutputPatch<VSOutput, 4> patch, float2 uv : SV_DomainLocation)
{
    float u = uv.x;
    float v = uv.y;

    float weights[4] = {
        (1.0f - u) * (1.0f - v),
        u * (1.0f - v),
        (1.0f - u) * v,
        u * v
    };

    VSOutput result;
    result.posH = float4(0.0f, 0.0f, 0.0f, 0.0f);
    result.posV = float3(0.0f, 0.0f, 0.0f);
    result.normalV = float3(0.0f, 0.0f, 0.0f);
    result.uv = float2(0.0f, 0.0f);

    for (int i = 0; i < 4; ++i)
    {
        result.posV += patch[i].posV * weights[i];
        result.normalV += patch[i].normalV * weights[i];
        result.uv += patch[i].uv * weights[i];
    }

    result.normalV = normalize(result.normalV);
    result.posH = float4(result.posV, 1.0f);

    return result;
}

