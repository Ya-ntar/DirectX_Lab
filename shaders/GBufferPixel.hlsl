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

    // Sample normal map and convert from DX format (0..1 -> -1..1)
    float4 normalMapSample = normalMapTex.Sample(baseColorSampler, input.uv);
    float3 normalFromMap = normalMapSample.rgb * 2.0f - 1.0f;  // Convert from [0,1] to [-1,1]

    // Check if normal map is valid (not a fallback white texture)
    // If the normal map sample is close to (1.0, 1.0, 1.0) in [0,1] range, it's likely a fallback
    bool hasNormalMap = !(abs(normalMapSample.r - 1.0f) < 0.01f && abs(normalMapSample.g - 1.0f) < 0.01f && abs(normalMapSample.b - 1.0f) < 0.01f);

    float3 finalNormal;
    if (hasNormalMap) {
        // Blend the sampled normal map with the mesh normal
        // This gives a more pronounced normal map effect while preserving geometry
        finalNormal = normalize(input.normalV + normalFromMap * 0.75f);
    } else {
        // Use mesh normal only
        finalNormal = input.normalV;
    }

    // Store position and normal in view-space coordinates
    // This is crucial for deferred rendering to work correctly
    o.posV = float4(input.posV, 1.0f);
    o.normalV = float4(normalize(finalNormal), 1.0f);
    o.albedoOut = float4(tex.rgb * albedo.rgb, 1.0f);
    return o;
}
