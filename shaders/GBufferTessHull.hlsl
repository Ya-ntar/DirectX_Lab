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
    float3 posW : TEXCOORD3;      // World-space position
    float3 normalW : TEXCOORD4;    // World-space normal
};

struct HSConstantData
{
    float edges[3] : SV_TessFactor;
    float inside : SV_InsideTessFactor;
};

HSConstantData HSConst(InputPatch<VSOutput, 3> patch)
{
    HSConstantData const_data;

    // Bias toward tessParams.x so "max" is a cap, not an implicit target (midpoint was too dense).
    float tess = tessParams.x + 0.32f * (tessParams.y - tessParams.x);
    tess = clamp(tess, tessParams.x, tessParams.y);
    tess = max(tess, 1.0f);

    // For triangles: 3 edge factors
    const_data.edges[0] = tess;
    const_data.edges[1] = tess;
    const_data.edges[2] = tess;
    // And 1 inside factor
    const_data.inside = tess;
    return const_data;
}

[domain("tri")]
[partitioning("fractional_even")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HSConst")]
VSOutput HSMain(InputPatch<VSOutput, 3> patch, uint cp_id : SV_OutputControlPointID)
{
    return patch[cp_id];
}