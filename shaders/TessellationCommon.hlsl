#ifndef TESSELLATION_COMMON_H
#define TESSELLATION_COMMON_H

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
    float4 tessParams;
    float displacement_scale;
    float3 _padding1;
    float time_seconds;
    float3 _padding0;
};

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

struct DSOutput
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float3 normalW : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

struct PSInput
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;
    float3 normalW : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

struct HSConstantData
{
    float edges[4] : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

#endif // TESSELLATION_COMMON_H
