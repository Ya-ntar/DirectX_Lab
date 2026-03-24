cbuffer GeometryCB : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;
    float4 albedo;
}

Texture2D baseColorTex : register(t0);
SamplerState baseColorSampler : register(s0);

struct VSOutput
{
    float4 posH : SV_POSITION;
    float3 posV : TEXCOORD0;
    float3 normalV : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

struct PSOutput
{
    float4 posV : SV_TARGET0;
    float4 normalV : SV_TARGET1;
    float4 albedoOut : SV_TARGET2;
};

PSOutput PSMain(VSOutput input)
{
    PSOutput o;
    float4 tex = baseColorTex.Sample(baseColorSampler, input.uv);
    o.posV = float4(input.posV, 1.0f);
    o.normalV = float4(normalize(input.normalV), 1.0f);
    o.albedoOut = float4(tex.rgb * albedo.rgb, 1.0f);
    return o;
}
