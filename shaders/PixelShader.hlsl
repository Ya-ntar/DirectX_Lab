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
    float timeSeconds;
    float3 _padding0;
}

Texture2D baseColorTex : register(t0);
SamplerState baseColorSampler : register(s0);

struct VSInput
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float3 normalW : TEXCOORD1;
};

float4 PSMain(VSOutput input) : SV_TARGET
{
    float3 N = normalize(input.normalW);
    float3 L = normalize(lightDirShininess.xyz);
    float3 V = normalize(cameraPos.xyz - input.posW);

    float ndotl = max(dot(N, L), 0.0f);
    float2 uv = frac(input.posW.xz * 0.25f);
    float2 uvAnim = frac(uv + float2(timeSeconds * 0.15f, -timeSeconds * 0.10f));
    float4 texSample = baseColorTex.Sample(baseColorSampler, uvAnim);
    float3 texProc = 0.5f + 0.5f * sin(float3(
        timeSeconds + uvAnim.x * 6.28318f,
        timeSeconds * 1.3f + uvAnim.y * 6.28318f,
        timeSeconds * 0.7f));
    float3 tex = texSample.rgb * texProc;
    float3 diffuse = (albedo.rgb * tex) * lightColor.rgb * ndotl;

    float3 R = reflect(-L, N);
    float specAngle = max(dot(R, V), 0.0f);
    float spec = pow(specAngle, max(lightDirShininess.w, 1.0f));
    float3 specular = lightColor.rgb * spec;

    float3 ambient = ambientColor.rgb * (albedo.rgb * tex);
    float3 color = ambient + diffuse + specular;
    float alpha = saturate(albedo.a * texSample.a);
    return float4(color, alpha);
}