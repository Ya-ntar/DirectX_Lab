#include "RenderingSystem.h"

#include <array>
#include <cstring>
#include <vector>
#include <d3dcompiler.h>
#include <iostream>

#include "framework/FrameworkInternal.h"

namespace gfw {
namespace {
constexpr UINT kCbAlign = 256;
UINT AlignCb(UINT size) {
    return (size + (kCbAlign - 1u)) & ~(kCbAlign - 1u);
}
}

bool RenderingSystem::Initialize(Framework *framework, UINT width, UINT height) {
    framework_ = framework;
    if (!framework_ || !framework_->GetDevice()) {
        return false;
    }
    std::cout << "Initializing GBuffer..." << std::endl;
    if (!gbuffer_.Initialize(framework_->GetDevice(), width, height)) {
        return false;
    }
    std::cout << "Creating geometry pipeline..." << std::endl;
    if (!CreateGeometryPipeline()) {
        return false;
    }
    std::cout << "Creating geometry tess pipeline..." << std::endl;
    if (!CreateGeometryTessPipeline()) {
        return false;
    }
    std::cout << "Creating lighting pipeline..." << std::endl;
    if (!CreateLightingPipeline()) {
        return false;
    }
    std::cout << "Creating GBuffer debug pipeline..." << std::endl;
    if (!CreateGBufferDebugPipeline()) {
        return false;
    }
    std::cout << "Creating constant buffers..." << std::endl;
    if (!CreateConstantBuffers()) {
        return false;
    }
    fallback_white_ = framework_->CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
    std::cout << "RenderingSystem initialized successfully." << std::endl;
    return true;
}

void RenderingSystem::Shutdown() {
    if (geometry_cb_) {
        geometry_cb_->Unmap(0, nullptr);
    }
    if (lighting_cb_) {
        lighting_cb_->Unmap(0, nullptr);
    }
    if (gbuffer_debug_cb_) {
        gbuffer_debug_cb_->Unmap(0, nullptr);
    }
    geometry_cb_mapped_ = nullptr;
    lighting_cb_mapped_ = nullptr;
    gbuffer_debug_cb_mapped_ = nullptr;
    geometry_cb_.Reset();
    lighting_cb_.Reset();
    gbuffer_debug_cb_.Reset();
    geometry_pso_.Reset();
    geometry_pso_wireframe_.Reset();
    geometry_root_sig_.Reset();
    geometry_tess_pso_.Reset();
    geometry_tess_pso_wireframe_.Reset();
    lighting_pso_.Reset();
    lighting_root_sig_.Reset();
    gbuffer_debug_pso_.Reset();
    gbuffer_debug_root_sig_.Reset();
    gbuffer_.Shutdown();
    fallback_white_.reset();
    framework_ = nullptr;
}

void RenderingSystem::SetDirectionalLight(const DirectionalLight &light) {
    directional_light_ = light;
}

void RenderingSystem::SetPointLights(const std::vector<PointLight> &lights) {
    point_lights_ = lights;
    if (point_lights_.size() > kMaxPointLights) {
        point_lights_.resize(kMaxPointLights);
    }
}

void RenderingSystem::SetSpotLights(const std::vector<SpotLight> &lights) {
    spot_lights_ = lights;
    if (spot_lights_.size() > kMaxSpotLights) {
        spot_lights_.resize(kMaxSpotLights);
    }
}

void RenderingSystem::Render(const std::vector<RenderObject> &objects, float) {
    if (!framework_ || !framework_->GetCommandList()) {
        return;
    }
    GeometryPass(objects);

    if (gbuffer_debug_mode_ != GBufferDebugMode::None) {
        GBufferDebugPass();
    } else {
        LightingPass();
    }
}

bool RenderingSystem::CreateGeometryPipeline() {
    ID3D12Device *device = framework_->GetDevice();
    UINT flags = 0;
#ifdef _DEBUG
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> ps_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    if (FAILED(D3DCompileFromFile(L"shaders/GBufferVertex.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0",
                                  flags, 0, &vs_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }
    error_blob.Reset();
    if (FAILED(D3DCompileFromFile(L"shaders/GBufferPixel.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0",
                                  flags, 0, &ps_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }

    // One table per texture (t0, t1). A single table with NumDescriptors=2 requires adjacent heap
    // slots; textures are allocated non-contiguously, so t1 would sample the wrong resource.
    D3D12_DESCRIPTOR_RANGE albedo_range = {};
    albedo_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    albedo_range.NumDescriptors = 1;
    albedo_range.BaseShaderRegister = 0;
    albedo_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE normal_range = {};
    normal_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    normal_range.NumDescriptors = 1;
    normal_range.BaseShaderRegister = 1;
    normal_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_params[3] = {};
    root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root_params[0].Descriptor.ShaderRegister = 0;
    root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[1].DescriptorTable.NumDescriptorRanges = 1;
    root_params[1].DescriptorTable.pDescriptorRanges = &albedo_range;
    root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    root_params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[2].DescriptorTable.NumDescriptorRanges = 1;
    root_params[2].DescriptorTable.pDescriptorRanges = &normal_range;
    root_params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
    rs_desc.NumParameters = 3;
    rs_desc.pParameters = root_params;
    rs_desc.NumStaticSamplers = 1;
    rs_desc.pStaticSamplers = &sampler;
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signature_blob;
    if (FAILED(D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob))) {
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(),
                                           IID_PPV_ARGS(&geometry_root_sig_)))) {
        return false;
    }

    const std::array<D3D12_INPUT_ELEMENT_DESC, 3> input_layout = {
        D3D12_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        D3D12_INPUT_ELEMENT_DESC{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        D3D12_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = geometry_root_sig_.Get();
    pso.VS = {vs_blob->GetBufferPointer(), vs_blob->GetBufferSize()};
    pso.PS = {ps_blob->GetBufferPointer(), ps_blob->GetBufferSize()};
    pso.InputLayout = {input_layout.data(), static_cast<UINT>(input_layout.size())};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleMask = UINT_MAX;
    pso.SampleDesc.Count = 1;
    pso.NumRenderTargets = GBuffer::kTargetCount;
    pso.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pso.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pso.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    D3D12_RASTERIZER_DESC rasterizer = {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    pso.RasterizerState = rasterizer;

    D3D12_BLEND_DESC blend = {};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
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
    for (UINT i = 0; i < 8; ++i) {
        blend.RenderTarget[i] = rt_blend;
    }
    pso.BlendState = blend;

    D3D12_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depth.StencilEnable = FALSE;
    pso.DepthStencilState = depth;

    std::cout << "Creating solid pipeline state..." << std::endl;
    if (!SUCCEEDED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&geometry_pso_)))) {
        std::cerr << "Failed to create solid pipeline state." << std::endl;
        return false;
    }

    // Create wireframe version
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_wireframe = pso;
    pso_wireframe.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    std::cout << "Creating wireframe pipeline state..." << std::endl;
    if (!SUCCEEDED(device->CreateGraphicsPipelineState(&pso_wireframe, IID_PPV_ARGS(&geometry_pso_wireframe_)))) {
        std::cerr << "Failed to create wireframe pipeline state." << std::endl;
        return false;
    }

    return true;
}

bool RenderingSystem::CreateGeometryTessPipeline() {
    ID3D12Device *device = framework_->GetDevice();
    UINT flags = 0;
#ifdef _DEBUG
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> hs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> ds_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> ps_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

    std::cout << "Compiling vertex shader..." << std::endl;
    // Compile vertex shader
    if (FAILED(D3DCompileFromFile(L"shaders/GBufferVertex.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0",
                                  flags, 0, &vs_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }
    error_blob.Reset();

    std::cout << "Compiling hull shader..." << std::endl;
    // Compile hull shader
    if (FAILED(D3DCompileFromFile(L"shaders/GBufferTessHull.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "HSMain", "hs_5_0",
                                  flags, 0, &hs_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }
    error_blob.Reset();

    std::cout << "Compiling domain shader..." << std::endl;
    // Compile domain shader
    if (FAILED(D3DCompileFromFile(L"shaders/GBufferTessDomain.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "DSMain", "ds_5_0",
                                  flags, 0, &ds_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }
    error_blob.Reset();

    std::cout << "Compiling pixel shader..." << std::endl;
    // Compile pixel shader
    if (FAILED(D3DCompileFromFile(L"shaders/GBufferPixel.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0",
                                  flags, 0, &ps_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }

    std::cout << "Shaders compiled successfully." << std::endl;

    // Texture2D SRVs must use descriptor tables; root SRV slots only support buffer SRVs.
    D3D12_DESCRIPTOR_RANGE srv_range_t0 = {};
    srv_range_t0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range_t0.NumDescriptors = 1;
    srv_range_t0.BaseShaderRegister = 0;
    srv_range_t0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE srv_range_t1 = {};
    srv_range_t1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range_t1.NumDescriptors = 1;
    srv_range_t1.BaseShaderRegister = 1;
    srv_range_t1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_DESCRIPTOR_RANGE srv_range_t2 = {};
    srv_range_t2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range_t2.NumDescriptors = 1;
    srv_range_t2.BaseShaderRegister = 2;
    srv_range_t2.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_params[4] = {};
    root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root_params[0].Descriptor.ShaderRegister = 0;
    root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[1].DescriptorTable.NumDescriptorRanges = 1;
    root_params[1].DescriptorTable.pDescriptorRanges = &srv_range_t0;
    root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    root_params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[2].DescriptorTable.NumDescriptorRanges = 1;
    root_params[2].DescriptorTable.pDescriptorRanges = &srv_range_t1;
    root_params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    root_params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[3].DescriptorTable.NumDescriptorRanges = 1;
    root_params[3].DescriptorTable.pDescriptorRanges = &srv_range_t2;
    root_params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
    rs_desc.NumParameters = 4;
    rs_desc.pParameters = root_params;
    rs_desc.NumStaticSamplers = 1;
    rs_desc.pStaticSamplers = &sampler;
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    std::cout << "Creating root signature..." << std::endl;
    Microsoft::WRL::ComPtr<ID3DBlob> signature_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> rs_error_blob;
    if (FAILED(D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &rs_error_blob))) {
        if (rs_error_blob) std::cerr << static_cast<const char *>(rs_error_blob->GetBufferPointer()) << std::endl;
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(),
                                           IID_PPV_ARGS(&geometry_tess_root_sig_)))) {
        std::cerr << "Failed to create root signature." << std::endl;
        return false;
    }

    std::cout << "Root signature created successfully." << std::endl;

    const std::array<D3D12_INPUT_ELEMENT_DESC, 3> input_layout = {
        D3D12_INPUT_ELEMENT_DESC{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        D3D12_INPUT_ELEMENT_DESC{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        D3D12_INPUT_ELEMENT_DESC{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = geometry_tess_root_sig_.Get();
    pso.VS = {vs_blob->GetBufferPointer(), vs_blob->GetBufferSize()};
    pso.HS = {hs_blob->GetBufferPointer(), hs_blob->GetBufferSize()};
    pso.DS = {ds_blob->GetBufferPointer(), ds_blob->GetBufferSize()};
    pso.PS = {ps_blob->GetBufferPointer(), ps_blob->GetBufferSize()};
    pso.InputLayout = {input_layout.data(), static_cast<UINT>(input_layout.size())};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    pso.SampleMask = UINT_MAX;
    pso.SampleDesc.Count = 1;
    pso.NumRenderTargets = GBuffer::kTargetCount;
    pso.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pso.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    pso.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    D3D12_RASTERIZER_DESC rasterizer = {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    pso.RasterizerState = rasterizer;

    D3D12_BLEND_DESC blend = {};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
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
    for (UINT i = 0; i < 8; ++i) {
        blend.RenderTarget[i] = rt_blend;
    }
    pso.BlendState = blend;

    D3D12_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = TRUE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depth.StencilEnable = FALSE;
    pso.DepthStencilState = depth;

    std::cout << "Creating solid pipeline state..." << std::endl;
    if (!SUCCEEDED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&geometry_tess_pso_)))) {
        std::cerr << "Failed to create solid pipeline state." << std::endl;
        return false;
    }

    // Create wireframe version
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_wireframe = pso;
    pso_wireframe.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    std::cout << "Creating wireframe pipeline state..." << std::endl;
    if (!SUCCEEDED(device->CreateGraphicsPipelineState(&pso_wireframe, IID_PPV_ARGS(&geometry_tess_pso_wireframe_)))) {
        std::cerr << "Failed to create wireframe pipeline state." << std::endl;
        return false;
    }

    return true;
}

bool RenderingSystem::CreateLightingPipeline() {
    ID3D12Device *device = framework_->GetDevice();
    UINT flags = 0;
#ifdef _DEBUG
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> ps_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    if (FAILED(D3DCompileFromFile(L"shaders/DeferredLighting.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0",
                                  flags, 0, &vs_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }
    error_blob.Reset();
    if (FAILED(D3DCompileFromFile(L"shaders/DeferredLighting.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0",
                                  flags, 0, &ps_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }

    D3D12_DESCRIPTOR_RANGE srv_range = {};
    srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range.NumDescriptors = 3;
    srv_range.BaseShaderRegister = 0;
    srv_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_params[2] = {};
    root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root_params[0].Descriptor.ShaderRegister = 0;
    root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[1].DescriptorTable.NumDescriptorRanges = 1;
    root_params[1].DescriptorTable.pDescriptorRanges = &srv_range;
    root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
    rs_desc.NumParameters = 2;
    rs_desc.pParameters = root_params;
    rs_desc.NumStaticSamplers = 1;
    rs_desc.pStaticSamplers = &sampler;
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signature_blob;
    if (FAILED(D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob))) {
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(),
                                           IID_PPV_ARGS(&lighting_root_sig_)))) {
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = lighting_root_sig_.Get();
    pso.VS = {vs_blob->GetBufferPointer(), vs_blob->GetBufferSize()};
    pso.PS = {ps_blob->GetBufferPointer(), ps_blob->GetBufferSize()};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleMask = UINT_MAX;
    pso.SampleDesc.Count = 1;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_RASTERIZER_DESC rasterizer = {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthClipEnable = TRUE;
    pso.RasterizerState = rasterizer;

    D3D12_BLEND_DESC blend = {};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
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
    blend.RenderTarget[0] = rt_blend;
    pso.BlendState = blend;

    D3D12_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depth.StencilEnable = FALSE;
    pso.DepthStencilState = depth;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&lighting_pso_)));
}

bool RenderingSystem::CreateGBufferDebugPipeline() {
    ID3D12Device *device = framework_->GetDevice();
    UINT flags = 0;
#ifdef _DEBUG
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> ps_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;
    if (FAILED(D3DCompileFromFile(L"shaders/GBufferDebug.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0",
                                  flags, 0, &vs_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }
    error_blob.Reset();
    if (FAILED(D3DCompileFromFile(L"shaders/GBufferDebug.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0",
                                  flags, 0, &ps_blob, &error_blob))) {
        if (error_blob) std::cerr << static_cast<const char *>(error_blob->GetBufferPointer()) << std::endl;
        return false;
    }

    D3D12_DESCRIPTOR_RANGE srv_range = {};
    srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range.NumDescriptors = 3;
    srv_range.BaseShaderRegister = 0;
    srv_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_params[2] = {};
    root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    root_params[0].Descriptor.ShaderRegister = 0;
    root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[1].DescriptorTable.NumDescriptorRanges = 1;
    root_params[1].DescriptorTable.pDescriptorRanges = &srv_range;
    root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
    rs_desc.NumParameters = 2;
    rs_desc.pParameters = root_params;
    rs_desc.NumStaticSamplers = 1;
    rs_desc.pStaticSamplers = &sampler;
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signature_blob;
    if (FAILED(D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob))) {
        return false;
    }
    if (FAILED(device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(),
                                           IID_PPV_ARGS(&gbuffer_debug_root_sig_)))) {
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = gbuffer_debug_root_sig_.Get();
    pso.VS = {vs_blob->GetBufferPointer(), vs_blob->GetBufferSize()};
    pso.PS = {ps_blob->GetBufferPointer(), ps_blob->GetBufferSize()};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleMask = UINT_MAX;
    pso.SampleDesc.Count = 1;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_RASTERIZER_DESC rasterizer = {};
    rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.CullMode = D3D12_CULL_MODE_NONE;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    pso.RasterizerState = rasterizer;

    D3D12_BLEND_DESC blend = {};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
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
    blend.RenderTarget[0] = rt_blend;
    pso.BlendState = blend;

    D3D12_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    depth.StencilEnable = FALSE;
    pso.DepthStencilState = depth;

    return SUCCEEDED(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&gbuffer_debug_pso_)));
}

bool RenderingSystem::CreateConstantBuffers() {
    const D3D12_HEAP_PROPERTIES heap = detail::HeapProperties(D3D12_HEAP_TYPE_UPLOAD);

    const UINT geometry_size = AlignCb(sizeof(GeometryCB));
    D3D12_RESOURCE_DESC g_desc = detail::BufferDesc(geometry_size);
    if (FAILED(framework_->GetDevice()->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &g_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&geometry_cb_)))) {
        return false;
    }
    void *g_mapped = nullptr;
    if (FAILED(geometry_cb_->Map(0, nullptr, &g_mapped))) {
        return false;
    }
    geometry_cb_mapped_ = static_cast<std::uint8_t *>(g_mapped);

    const UINT lighting_size = AlignCb(sizeof(LightingCB));
    D3D12_RESOURCE_DESC l_desc = detail::BufferDesc(lighting_size);
    if (FAILED(framework_->GetDevice()->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &l_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&lighting_cb_)))) {
        return false;
    }
    void *l_mapped = nullptr;
    if (FAILED(lighting_cb_->Map(0, nullptr, &l_mapped))) {
        return false;
    }
    lighting_cb_mapped_ = static_cast<std::uint8_t *>(l_mapped);

    const UINT gbuffer_debug_size = AlignCb(sizeof(GBufferDebugCB));
    D3D12_RESOURCE_DESC gb_desc = detail::BufferDesc(gbuffer_debug_size);
    if (FAILED(framework_->GetDevice()->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &gb_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&gbuffer_debug_cb_)))) {
        return false;
    }
    void *gb_mapped = nullptr;
    if (FAILED(gbuffer_debug_cb_->Map(0, nullptr, &gb_mapped))) {
        return false;
    }
    gbuffer_debug_cb_mapped_ = static_cast<std::uint8_t *>(gb_mapped);

    return true;
}

void RenderingSystem::GeometryPass(const std::vector<RenderObject> &objects) {
    ID3D12GraphicsCommandList *cmd = framework_->GetCommandList();
    const auto &scene = framework_->GetSceneState();

    gbuffer_.TransitionToRenderTargets(cmd);
    const std::array<D3D12_CPU_DESCRIPTOR_HANDLE, GBuffer::kTargetCount> rtvs = {
        gbuffer_.GetRtv(0), gbuffer_.GetRtv(1), gbuffer_.GetRtv(2)
    };
    const D3D12_CPU_DESCRIPTOR_HANDLE dsv = framework_->GetDepthDsvHandle();
    cmd->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, &dsv);
    gbuffer_.Clear(cmd);
    cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Choose pipeline based on tessellation setting
    if (tessellation_enabled_) {
        cmd->SetGraphicsRootSignature(geometry_tess_root_sig_.Get());
        if (render_mode_ == RenderMode::Wireframe) {
            cmd->SetPipelineState(geometry_tess_pso_wireframe_.Get());
        } else {
            cmd->SetPipelineState(geometry_tess_pso_.Get());
        }
    } else {
        cmd->SetGraphicsRootSignature(geometry_root_sig_.Get());
        if (render_mode_ == RenderMode::Wireframe) {
            cmd->SetPipelineState(geometry_pso_wireframe_.Get());
        } else {
            cmd->SetPipelineState(geometry_pso_.Get());
        }
    }

    ID3D12DescriptorHeap *heaps[] = {framework_->GetSrvHeap()};
    cmd->SetDescriptorHeaps(1, heaps);

    const float aspect = framework_->GetViewport().Width / framework_->GetViewport().Height;
    const DirectX::XMMATRIX view = scene.camera.ViewMatrix();
    const DirectX::XMMATRIX proj = scene.projection.Matrix(aspect);

    for (const RenderObject &obj : objects) {
        if (!obj.mesh) {
            continue;
        }

        GeometryCB cb = {};
        DirectX::XMStoreFloat4x4(&cb.world, DirectX::XMLoadFloat4x4(&obj.world));
        DirectX::XMStoreFloat4x4(&cb.view, view);
        DirectX::XMStoreFloat4x4(&cb.proj, proj);
        cb.albedo = obj.albedo;
        cb.tess_params = {tessellation_min_, tessellation_max_, tessellation_near_dist_, tessellation_far_dist_};
        cb.camera_pos = {scene.camera.position.x, scene.camera.position.y, scene.camera.position.z, 0.0f};
        std::memcpy(geometry_cb_mapped_, &cb, sizeof(cb));

        cmd->SetGraphicsRootConstantBufferView(0, geometry_cb_->GetGPUVirtualAddress());

        // Bind textures based on pipeline type
        D3D12_GPU_DESCRIPTOR_HANDLE base_srv = (obj.texture ? obj.texture : fallback_white_)->srv_gpu;

        if (tessellation_enabled_) {
            // For tessellation pipeline: bind base color, normal, and displacement textures
            D3D12_GPU_DESCRIPTOR_HANDLE normal_srv = (obj.normal_texture ? obj.normal_texture : fallback_white_)->srv_gpu;
            D3D12_GPU_DESCRIPTOR_HANDLE displacement_srv = (obj.displacement_texture ? obj.displacement_texture : fallback_white_)->srv_gpu;
            cmd->SetGraphicsRootDescriptorTable(1, base_srv);
            cmd->SetGraphicsRootDescriptorTable(2, normal_srv);
            cmd->SetGraphicsRootDescriptorTable(3, displacement_srv);
        } else {
            D3D12_GPU_DESCRIPTOR_HANDLE normal_srv = (obj.normal_texture ? obj.normal_texture : fallback_white_)->srv_gpu;
            cmd->SetGraphicsRootDescriptorTable(1, base_srv);
            cmd->SetGraphicsRootDescriptorTable(2, normal_srv);
        }

        // Set primitive topology based on tessellation state
        D3D_PRIMITIVE_TOPOLOGY topo = obj.mesh->topology;
        if (tessellation_enabled_) {
            // Use 3-control-point patch list for triangle tessellation
            topo = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
        }
        cmd->IASetPrimitiveTopology(topo);
        cmd->IASetVertexBuffers(0, 1, &obj.mesh->vertex_buffer_view);

        if (obj.mesh->index_buffer) {
            cmd->IASetIndexBuffer(&obj.mesh->index_buffer_view);
            cmd->DrawIndexedInstanced(obj.mesh->index_count, 1, 0, 0, 0);
        } else {
            cmd->DrawInstanced(obj.mesh->vertex_count, 1, 0, 0);
        }
    }
    gbuffer_.TransitionToShaderResources(cmd);
}

void RenderingSystem::LightingPass() {
    ID3D12GraphicsCommandList *cmd = framework_->GetCommandList();
    const auto &scene = framework_->GetSceneState();
    const DirectX::XMMATRIX view = scene.camera.ViewMatrix();

    D3D12_CPU_DESCRIPTOR_HANDLE back_rtv = framework_->GetCurrentBackBufferRtv();
    cmd->OMSetRenderTargets(1, &back_rtv, FALSE, nullptr);
    const float clear_color[4] = {0.02f, 0.02f, 0.03f, 1.0f};
    cmd->ClearRenderTargetView(back_rtv, clear_color, 0, nullptr);

    LightingCB cb = {};

    // ===== IMPORTANT: All light positions and directions MUST be in VIEW-SPACE =====
    // This is critical for deferred rendering to work correctly:
    // - GBuffer contains positions and normals in VIEW-SPACE
    // - Lights must also be in VIEW-SPACE to match
    // - No additional transformations needed in lighting shader

    // Transform directional light direction from world-space to view-space
    const DirectX::XMVECTOR dir_world = DirectX::XMVectorSet(
        directional_light_.direction.x,
        directional_light_.direction.y,
        directional_light_.direction.z,
        0.0f);
    const DirectX::XMVECTOR dir_view = DirectX::XMVector3Normalize(DirectX::XMVector4Transform(dir_world, view));
    DirectX::XMFLOAT3 dir_view_f3 = {};
    DirectX::XMStoreFloat3(&dir_view_f3, dir_view);
    cb.dir_light_dir = {dir_view_f3.x, dir_view_f3.y, dir_view_f3.z, 0.0f};
    cb.dir_light_color_intensity = {directional_light_.color.x, directional_light_.color.y, directional_light_.color.z, 1.0f};
    cb.ambient_color = directional_light_.ambient;
    cb.point_count = static_cast<UINT>(point_lights_.size());
    cb.spot_count = static_cast<UINT>(spot_lights_.size());

    // Transform point light positions from world-space to view-space
    for (UINT i = 0; i < cb.point_count; ++i) {
        const DirectX::XMVECTOR point_world = DirectX::XMVectorSet(
            point_lights_[i].position.x,
            point_lights_[i].position.y,
            point_lights_[i].position.z,
            1.0f);
        const DirectX::XMVECTOR point_view = DirectX::XMVector4Transform(point_world, view);
        DirectX::XMFLOAT3 point_view_f3 = {};
        DirectX::XMStoreFloat3(&point_view_f3, point_view);
        cb.point_lights[i].pos_range = {point_view_f3.x, point_view_f3.y, point_view_f3.z, point_lights_[i].range};
        cb.point_lights[i].color_intensity = {point_lights_[i].color.x, point_lights_[i].color.y, point_lights_[i].color.z, point_lights_[i].intensity};
    }

    // Transform spot light positions and directions from world-space to view-space
    for (UINT i = 0; i < cb.spot_count; ++i) {
        const DirectX::XMVECTOR spot_pos_world = DirectX::XMVectorSet(
            spot_lights_[i].position.x,
            spot_lights_[i].position.y,
            spot_lights_[i].position.z,
            1.0f);
        const DirectX::XMVECTOR spot_pos_view = DirectX::XMVector4Transform(spot_pos_world, view);
        DirectX::XMFLOAT3 spot_pos_view_f3 = {};
        DirectX::XMStoreFloat3(&spot_pos_view_f3, spot_pos_view);
        cb.spot_lights[i].pos_range = {spot_pos_view_f3.x, spot_pos_view_f3.y, spot_pos_view_f3.z, spot_lights_[i].range};

        const DirectX::XMVECTOR spot_dir_world = DirectX::XMVectorSet(
            spot_lights_[i].direction.x,
            spot_lights_[i].direction.y,
            spot_lights_[i].direction.z,
            0.0f);
        const DirectX::XMVECTOR spot_dir_view = DirectX::XMVector3Normalize(DirectX::XMVector4Transform(spot_dir_world, view));
        DirectX::XMFLOAT3 spot_dir_view_f3 = {};
        DirectX::XMStoreFloat3(&spot_dir_view_f3, spot_dir_view);
        cb.spot_lights[i].dir_angle_cos = {spot_dir_view_f3.x, spot_dir_view_f3.y, spot_dir_view_f3.z, spot_lights_[i].angle_cos};
        cb.spot_lights[i].color_intensity = {spot_lights_[i].color.x, spot_lights_[i].color.y, spot_lights_[i].color.z, spot_lights_[i].intensity};
    }
    std::memcpy(lighting_cb_mapped_, &cb, sizeof(cb));

    cmd->SetGraphicsRootSignature(lighting_root_sig_.Get());
    cmd->SetPipelineState(lighting_pso_.Get());
    ID3D12DescriptorHeap *heaps[] = {gbuffer_.GetSrvHeap()};
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootConstantBufferView(0, lighting_cb_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootDescriptorTable(1, gbuffer_.GetSrv(0));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(3, 1, 0, 0);
}

void RenderingSystem::GBufferDebugPass() {
    ID3D12GraphicsCommandList *cmd = framework_->GetCommandList();

    D3D12_CPU_DESCRIPTOR_HANDLE back_rtv = framework_->GetCurrentBackBufferRtv();
    cmd->OMSetRenderTargets(1, &back_rtv, FALSE, nullptr);
    const float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmd->ClearRenderTargetView(back_rtv, clear_color, 0, nullptr);

    GBufferDebugCB cb = {};
    cb.mode = static_cast<INT>(gbuffer_debug_mode_);
    std::memcpy(gbuffer_debug_cb_mapped_, &cb, sizeof(cb));

    cmd->SetGraphicsRootSignature(gbuffer_debug_root_sig_.Get());
    cmd->SetPipelineState(gbuffer_debug_pso_.Get());
    ID3D12DescriptorHeap *heaps[] = {gbuffer_.GetSrvHeap()};
    cmd->SetDescriptorHeaps(1, heaps);
    cmd->SetGraphicsRootConstantBufferView(0, gbuffer_debug_cb_->GetGPUVirtualAddress());
    // Pass all three GBuffer textures (Position, Normal, Albedo) at once
    // The descriptor table starts at index 0 which contains all three SRVs
    cmd->SetGraphicsRootDescriptorTable(1, gbuffer_.GetSrv(0));
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->DrawInstanced(3, 1, 0, 0);
}

void PushLightsToRenderingSystem(const LightControlState &state, RenderingSystem &rendering) {
    rendering.SetDirectionalLight(state.directional);
    rendering.SetPointLights(std::vector<PointLight>(
        state.point_lights.begin(),
        state.point_lights.begin() + state.enabled_point_count));
    rendering.SetSpotLights(std::vector<SpotLight>(
        state.spot_lights.begin(),
        state.spot_lights.begin() + state.enabled_spot_count));
}
}
