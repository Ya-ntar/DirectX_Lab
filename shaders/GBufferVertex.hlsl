cbuffer GeometryCB : register(b0)
{
    row_major float4x4 world;
    row_major float4x4 view;
    row_major float4x4 proj;
    float4 albedo;
}

struct VSInput
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 posH : SV_POSITION;
    float3 posV : TEXCOORD0;
    float3 normalV : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput o;
    float4 posW = mul(float4(input.pos, 1.0f), world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.posV = posV.xyz;
    float3 normalW = mul(float4(input.normal, 0.0f), world).xyz;
    o.normalV = mul(float4(normalW, 0.0f), view).xyz;
    o.uv = input.uv;
    return o;
}
