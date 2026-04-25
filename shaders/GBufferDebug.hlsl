cbuffer DebugCB : register(b0)
{
    int mode;
    float3 _pad;
};

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

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 result = float4(0.0f, 0.0f, 0.0f, 1.0f);

    if (mode == 0)
    {
        // Position visualization
        float4 pos = gPosition.Sample(gSampler, input.uv);
        result = float4(abs(pos.xyz) * 0.5f, 1.0f);
    }
    else if (mode == 1)
    {
        // Normal visualization
        float4 normal = gNormal.Sample(gSampler, input.uv);
        result = float4(normal.xyz * 0.5f + 0.5f, 1.0f);
    }
    else if (mode == 2)
    {
        // Albedo visualization
        result = gAlbedo.Sample(gSampler, input.uv);
    }
    else if (mode == 3)
    {
        // Depth visualization (Z component of position in view-space)
        // In view-space, Z is negative (camera looks down -Z axis)
        float4 pos = gPosition.Sample(gSampler, input.uv);
        float depth = abs(pos.z) / 50.0f;  // Normalize to reasonable range
        depth = clamp(depth, 0.0f, 1.0f);
        result = float4(depth, depth, depth, 1.0f);  // Grayscale gradient
    }

    return result;
}


