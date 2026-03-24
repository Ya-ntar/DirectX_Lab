cbuffer SceneCB : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;
    float4 lightDirShininess;
    float4 cameraPos;
    float4 lightColor;
    float4 ambientColor;
    float4 albedo;
    float4 uvParams;
    float timeSeconds;
    float3 _padding0;
}

Texture2D baseColorTex : register(t0);
SamplerState baseColorSampler : register(s0);

struct VSInput
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float3 normalW : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput o;
    float4 posW = mul(float4(input.pos, 1.0f), world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.posW = posW.xyz;
    o.normalW = mul(float4(input.normal, 0.0f), world).xyz;
    o.uv = input.uv;
    return o;
}