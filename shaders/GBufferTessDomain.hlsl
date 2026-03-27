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
Texture2D displacementTex : register(t2);
SamplerState baseColorSampler : register(s0);

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

[domain("tri")]
VSOutput DSMain(HSConstantData patch_data, const OutputPatch<VSOutput, 3> patch, float3 bary : SV_DomainLocation)
{
    // bary contains barycentric coordinates (u, v, w) where u + v + w = 1.0
    float u = bary.x;
    float v = bary.y;
    float w = bary.z;

    // Interpolate in world-space using barycentric coordinates
    float3 interpolated_posW = patch[0].posW * u + patch[1].posW * v + patch[2].posW * w;
    float3 interpolated_normalW = patch[0].normalW * u + patch[1].normalW * v + patch[2].normalW * w;
    float2 interpolated_uv = patch[0].uv * u + patch[1].uv * v + patch[2].uv * w;

    // Normalize the interpolated world-space normal
    float3 normalW = normalize(interpolated_normalW);

    // Sample displacement texture
    float4 displacementSample = displacementTex.SampleLevel(baseColorSampler, interpolated_uv, 0);
    bool hasDisplacement = !(abs(displacementSample.r - 1.0f) < 0.01f && abs(displacementSample.g - 1.0f) < 0.01f && abs(displacementSample.b - 1.0f) < 0.01f);
    float displacementValue = 0.0f;
    if (hasDisplacement) {
        displacementValue = (displacementSample.r - 0.5f) * tessParams.z;
    }

    // Sample normal map
    float4 normalMapSample = normalMapTex.SampleLevel(baseColorSampler, interpolated_uv, 0);
    bool hasNormalMap = !(abs(normalMapSample.r - 1.0f) < 0.01f && abs(normalMapSample.g - 1.0f) < 0.01f && abs(normalMapSample.b - 1.0f) < 0.01f);

    // Determine displacement direction
    float3 displacementNormal = normalW;
    if (hasNormalMap) {
        float3 normalFromMap = normalMapSample.rgb * 2.0f - 1.0f;
        // Blend with normal map for more detailed displacement
        displacementNormal = normalize(normalW + normalFromMap * 0.5f);
    }

    // Apply displacement
    interpolated_posW += displacementNormal * displacementValue;

    // Transform displaced world-space position to view-space
    float4 posV = mul(float4(interpolated_posW, 1.0f), view);

    // Transform world-space normal to view-space
    float3 normalV = mul(float4(normalW, 0.0f), view).xyz;

    VSOutput result;
    result.posW = interpolated_posW;
    result.posV = posV.xyz;
    result.normalW = normalW;
    result.normalV = normalV;
    result.uv = interpolated_uv;

    // Apply projection matrix to get screen-space position
    result.posH = mul(posV, proj);

    return result;
}
