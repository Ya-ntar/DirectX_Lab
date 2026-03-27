#include "TessellationCommon.hlsl"

 [domain("quad")]
DSOutput DSMain(HSConstantData patch_data, const OutputPatch<VSOutput, 4> patch, float2 uv : SV_DomainLocation) {
    float u = uv.x;
    float v = uv.y;
    float3 pos = float3(0.0f, 0.0f, 0.0f);
    float3 normal = float3(0.0f, 0.0f, 0.0f);
    float2 tex = float2(0.0f, 0.0f);

    float weights[4] = {
        (1.0f - u) * (1.0f - v),
        u * (1.0f - v),
        (1.0f - u) * v,
        u * v
    };

    pos += patch[0].posW * weights[0];
    pos += patch[1].posW * weights[1];
    pos += patch[2].posW * weights[2];
    pos += patch[3].posW * weights[3];

    normal += patch[0].normalW * weights[0];
    normal += patch[1].normalW * weights[1];
    normal += patch[2].normalW * weights[2];
    normal += patch[3].normalW * weights[3];

    tex += patch[0].uv * weights[0];
    tex += patch[1].uv * weights[1];
    tex += patch[2].uv * weights[2];
    tex += patch[3].uv * weights[3];

    DSOutput result;
    result.posW = pos;
    result.normalW = normalize(normal);
    result.uv = tex;
    float4 worldPos = mul(float4(result.posW, 1.0f), world);
    float4 viewPos = mul(worldPos, view);
    result.posH = mul(viewPos, proj);
    return result;
}
