#include "Framework.h"

#include <iterator>

namespace gfw {
    namespace {
        const char *kPhongShaderSource = R"(
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
};

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

VSOutput VSMain(VSInput input)
{
    VSOutput o;
    float4 posW = mul(float4(input.pos, 1.0f), world);
    float4 posV = mul(posW, view);
    o.posH = mul(posV, proj);
    o.posW = posW.xyz;
    o.normalW = mul(float4(input.normal, 0.0f), world).xyz;
    return o;
}

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
)";
    }

    bool Framework::CreatePhongPipeline() {
        UINT compile_flags = 0;
#ifdef _DEBUG
        compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        ComPtr<ID3DBlob> vs_blob;
        ComPtr<ID3DBlob> ps_blob;
        ComPtr<ID3DBlob> error_blob;

        HRESULT hr = D3DCompile(
                kPhongShaderSource,
                std::strlen(kPhongShaderSource),
                nullptr,
                nullptr,
                nullptr,
                "VSMain",
                "vs_5_0",
                compile_flags,
                0,
                &vs_blob,
                &error_blob);

        if (FAILED(hr)) {
            if (error_blob) {
                std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
            }
            std::wcerr << L"Failed to compile vertex shader!" << std::endl;
            return false;
        }

        error_blob.Reset();
        hr = D3DCompile(
                kPhongShaderSource,
                std::strlen(kPhongShaderSource),
                nullptr,
                nullptr,
                nullptr,
                "PSMain",
                "ps_5_0",
                compile_flags,
                0,
                &ps_blob,
                &error_blob);

        if (FAILED(hr)) {
            if (error_blob) {
                std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
            }
            std::wcerr << L"Failed to compile pixel shader!" << std::endl;
            return false;
        }

        D3D12_DESCRIPTOR_RANGE srv_range = {};
        srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srv_range.NumDescriptors = 1;
        srv_range.BaseShaderRegister = 0;
        srv_range.RegisterSpace = 0;
        srv_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER root_params[2] = {};
        root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root_params[0].Descriptor.ShaderRegister = 0;
        root_params[0].Descriptor.RegisterSpace = 0;
        root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_params[1].DescriptorTable.NumDescriptorRanges = 1;
        root_params[1].DescriptorTable.pDescriptorRanges = &srv_range;
        root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MipLODBias = 0.0f;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC root_desc = {};
        root_desc.NumParameters = static_cast<UINT>(std::size(root_params));
        root_desc.pParameters = root_params;
        root_desc.NumStaticSamplers = 1;
        root_desc.pStaticSamplers = &sampler;
        root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature_blob;
        error_blob.Reset();
        if (FAILED(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob,
                                               &error_blob))) {
            if (error_blob) {
                std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
            }
            std::wcerr << L"Failed to serialize RootSignature!" << std::endl;
            return false;
        }

        if (FAILED(device_->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(),
                                                IID_PPV_ARGS(&root_signature_)))) {
            std::wcerr << L"Failed to create RootSignature!" << std::endl;
            return false;
        }

        const std::array<D3D12_INPUT_ELEMENT_DESC, 2> input_layout = {
                D3D12_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                                         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                D3D12_INPUT_ELEMENT_DESC{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
                                         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };

        D3D12_RASTERIZER_DESC rasterizer = {};
        rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizer.CullMode = D3D12_CULL_MODE_BACK;
        rasterizer.FrontCounterClockwise = FALSE;
        rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizer.DepthClipEnable = TRUE;
        rasterizer.MultisampleEnable = FALSE;
        rasterizer.AntialiasedLineEnable = FALSE;
        rasterizer.ForcedSampleCount = 0;
        rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_RENDER_TARGET_BLEND_DESC rt_blend = {};
        rt_blend.BlendEnable = FALSE;
        rt_blend.LogicOpEnable = FALSE;
        rt_blend.SrcBlend = D3D12_BLEND_ONE;
        rt_blend.DestBlend = D3D12_BLEND_ZERO;
        rt_blend.BlendOp = D3D12_BLEND_OP_ADD;
        rt_blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt_blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        rt_blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt_blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        rt_blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_BLEND_DESC blend = {};
        blend.AlphaToCoverageEnable = FALSE;
        blend.IndependentBlendEnable = FALSE;
        blend.RenderTarget[0] = rt_blend;

        D3D12_DEPTH_STENCIL_DESC depth_stencil = {};
        depth_stencil.DepthEnable = TRUE;
        depth_stencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depth_stencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depth_stencil.StencilEnable = FALSE;
        depth_stencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        depth_stencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.pRootSignature = root_signature_.Get();
        pso_desc.VS = {vs_blob->GetBufferPointer(), vs_blob->GetBufferSize()};
        pso_desc.PS = {ps_blob->GetBufferPointer(), ps_blob->GetBufferSize()};
        pso_desc.BlendState = blend;
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.RasterizerState = rasterizer;
        pso_desc.DepthStencilState = depth_stencil;
        pso_desc.InputLayout = {input_layout.data(), static_cast<UINT>(input_layout.size())};
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pso_desc.SampleDesc.Count = 1;

        if (FAILED(device_->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state_)))) {
            std::wcerr << L"Failed to create PSO!" << std::endl;
            return false;
        }

        D3D12_RENDER_TARGET_BLEND_DESC rt_blend_transparent = rt_blend;
        rt_blend_transparent.BlendEnable = TRUE;
        rt_blend_transparent.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt_blend_transparent.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt_blend_transparent.BlendOp = D3D12_BLEND_OP_ADD;
        rt_blend_transparent.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt_blend_transparent.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt_blend_transparent.BlendOpAlpha = D3D12_BLEND_OP_ADD;

        D3D12_BLEND_DESC blend_transparent = blend;
        blend_transparent.RenderTarget[0] = rt_blend_transparent;

        D3D12_DEPTH_STENCIL_DESC depth_stencil_transparent = depth_stencil;
        depth_stencil_transparent.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc_transparent = pso_desc;
        pso_desc_transparent.BlendState = blend_transparent;
        pso_desc_transparent.DepthStencilState = depth_stencil_transparent;

        if (FAILED(device_->CreateGraphicsPipelineState(&pso_desc_transparent,
                                                        IID_PPV_ARGS(&pipeline_state_transparent_)))) {
            std::wcerr << L"Failed to create transparent PSO!" << std::endl;
            return false;
        }

        return true;
    }
}

