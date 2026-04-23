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
    float4 effectParams;
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

float3 ApplyVertexWave(float3 localPos)
{
    const float amplitude = max(effectParams.z, 0.0f);
    if (amplitude <= 0.0f)
    {
        return localPos;
    }

    const float frequency = max(effectParams.w, 0.001f);
    const float phase = timeSeconds * frequency + (localPos.x + localPos.z) * 2.0f;
    const float displacement = sin(phase) * amplitude;
    return float3(localPos.x, localPos.y + displacement, localPos.z);
}

VSOutput VSMain(VSInput input)
{
    VSOutput o;
    const float3 animatedLocalPos = ApplyVertexWave(input.pos);
    float4 posW = mul(float4(animatedLocalPos, 1.0f), world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.posW = posW.xyz;
    o.normalW = mul(float4(input.normal, 0.0f), world).xyz;
    o.uv = input.uv;
    return o;
}