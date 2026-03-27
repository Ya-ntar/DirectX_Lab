cbuffer GeometryCB : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;
    float4 albedo;
    float4 tessParams;
};

Texture2D baseColorTex : register(t0);
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
    // Store position and normal in view-space coordinates
    // This is crucial for deferred rendering to work correctly
    o.posV = float4(input.posV, 1.0f);
    o.normalV = float4(normalize(input.normalV), 1.0f);
    o.albedoOut = float4(tex.rgb * albedo.rgb, 1.0f);
    return o;
}
