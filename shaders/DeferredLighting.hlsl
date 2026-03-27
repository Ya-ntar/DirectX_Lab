#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS 8

struct PointLightGpu
{
    float4 posRange;
    float4 colorIntensity;
};

struct SpotLightGpu
{
    float4 posRange;
    float4 dirAngleCos;
    float4 colorIntensity;
};

cbuffer LightingCB : register(b0)
{
    // All light positions and directions are in VIEW-SPACE coordinates
    // This allows us to compute lighting directly without additional transformations
    float4 dirLightDir;              // Direction of directional light (in view-space)
    float4 dirLightColorIntensity;   // Color and intensity of directional light
    float4 ambientColor;             // Ambient contribution
    PointLightGpu pointLights[MAX_POINT_LIGHTS];   // Point lights (positions in view-space)
    SpotLightGpu spotLights[MAX_SPOT_LIGHTS];      // Spot lights (positions/directions in view-space)
    uint pointCount;
    uint spotCount;
    float2 _pad;
}

Texture2D gPosition : register(t0);
Texture2D gNormal : register(t1);
Texture2D gAlbedo : register(t2);
SamplerState gSampler : register(s0);

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint vertexId : SV_VertexID)
{
    VSOutput o;
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };
    o.pos = float4(positions[vertexId], 0.0f, 1.0f);
    o.uv = o.pos.xy * float2(0.5f, -0.5f) + 0.5f;
    return o;
}

float3 EvaluateDirectional(float3 N, float3 V, float3 albedo)
{
    float3 L = normalize(-dirLightDir.xyz);
    float nDotL = saturate(dot(N, L));
    float3 H = normalize(L + V);
    float spec = pow(saturate(dot(N, H)), 32.0f);
    float3 diffuse = albedo * dirLightColorIntensity.rgb * nDotL * dirLightColorIntensity.a;
    return diffuse + spec * dirLightColorIntensity.rgb * 0.2f;
}

float3 EvaluatePoint(float3 N, float3 posV, float3 albedo, PointLightGpu light)
{
    float3 toLight = light.posRange.xyz - posV;
    float dist = length(toLight);
    if (dist > light.posRange.w) return 0.0f;
    float3 L = toLight / max(dist, 1e-4f);
    float atten = saturate(1.0f - dist / light.posRange.w);
    float nDotL = saturate(dot(N, L));
    return albedo * light.colorIntensity.rgb * (nDotL * atten * light.colorIntensity.a);
}

float3 EvaluateSpot(float3 N, float3 posV, float3 albedo, SpotLightGpu light)
{
    float3 toLight = light.posRange.xyz - posV;
    float dist = length(toLight);
    if (dist > light.posRange.w) return 0.0f;
    float3 L = normalize(toLight);
    float3 spotDir = normalize(-light.dirAngleCos.xyz);
    float cone = saturate((dot(L, spotDir) - light.dirAngleCos.w) / max(1.0f - light.dirAngleCos.w, 1e-4f));
    float nDotL = saturate(dot(N, L));
    float atten = saturate(1.0f - dist / light.posRange.w);
    return albedo * light.colorIntensity.rgb * (nDotL * atten * cone * light.colorIntensity.a);
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    // All data from GBuffer is in VIEW-SPACE coordinates
    float3 posV = gPosition.Sample(gSampler, input.uv).xyz;  // Pixel position in view-space
    float3 normalV = normalize(gNormal.Sample(gSampler, input.uv).xyz);  // Pixel normal in view-space
    float3 albedo = gAlbedo.Sample(gSampler, input.uv).rgb;
    if (dot(normalV, normalV) < 1e-5f) {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // View direction: from pixel towards camera (camera is at origin in view-space)
    float3 V = normalize(-posV);
    float3 color = ambientColor.rgb * albedo;
    color += EvaluateDirectional(normalV, V, albedo);

    [loop]
    for (uint i = 0; i < pointCount; ++i) {
        color += EvaluatePoint(normalV, posV, albedo, pointLights[i]);
    }
    [loop]
    for (uint i = 0; i < spotCount; ++i) {
        color += EvaluateSpot(normalV, posV, albedo, spotLights[i]);
    }
    return float4(color, 1.0f);
}
