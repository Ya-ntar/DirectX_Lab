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

HSConstantData HSConst(InputPatch<VSOutput, 4> patch)
{
    HSConstantData const_data;

    float tess = clamp((tessParams.x + tessParams.y) * 0.5f, tessParams.x, tessParams.y);
    tess = max(tess, 1.0f);

    const_data.edges[0] = tess;
    const_data.edges[1] = tess;
    const_data.edges[2] = tess;
    const_data.edges[3] = tess;
    const_data.inside[0] = tess;
    const_data.inside[1] = tess;
    return const_data;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("HSConst")]
VSOutput HSMain(InputPatch<VSOutput, 4> patch, uint cp_id : SV_OutputControlPointID)
{
    return patch[cp_id];
}


