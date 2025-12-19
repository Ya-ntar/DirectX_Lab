
#include <iterator>

#include "Framework.h"

#ifdef _DEBUG

#include <dxgidebug.h>

#endif

namespace gfw {
    namespace {

        constexpr UINT Align256(UINT size) {
            return (size + 255u) & ~255u;
        }

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
};

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
    float3 diffuse = albedo.rgb * lightColor.rgb * ndotl;

    float3 R = reflect(-L, N);
    float specAngle = max(dot(R, V), 0.0f);
    float spec = pow(specAngle, max(lightDirShininess.w, 1.0f));
    float3 specular = lightColor.rgb * spec;

    float3 ambient = ambientColor.rgb * albedo.rgb;
    float3 color = ambient + diffuse + specular;
    return float4(color, 1.0f);
}
)";

        D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type) {
            D3D12_HEAP_PROPERTIES props = {};
            props.Type = type;
            props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            props.CreationNodeMask = 1;
            props.VisibleNodeMask = 1;
            return props;
        }

        D3D12_RESOURCE_DESC BufferDesc(UINT64 size) {
            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Alignment = 0;
            desc.Width = size;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;
            return desc;
        }
    }

    Framework::Framework() = default;

    Framework::~Framework() {
        Shutdown();
    }

    bool Framework::Initialize(Window *window) {
        if (!window) {
            std::wcerr << L"Framework::Initialize: Window pointer is null!" << std::endl;
            return false;
        }

        window_ = window;

        UINT dxgi_factory_flags = 0;

#ifdef _DEBUG
        {
            ComPtr<ID3D12Debug> debug_controller;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
                debug_controller->EnableDebugLayer();
                dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif

        if (FAILED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory_)))) {
            std::wcerr << L"Failed to create DXGI Factory!" << std::endl;
            return false;
        }

        ComPtr<IDXGIAdapter1> adapter;
        for (UINT adapter_index = 0;
             DXGI_ERROR_NOT_FOUND != factory_->EnumAdapters1(adapter_index, &adapter); ++adapter_index) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }

            ComPtr<ID3D12Device> test_device;
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&test_device)))) {
                break;
            }
        }

        if (!adapter) {
            factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        }

        if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) {
            std::wcerr << L"Failed to create D3D12 Device!" << std::endl;
            return false;
        }

        D3D12_COMMAND_QUEUE_DESC queue_desc = {};
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        if (FAILED(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue_)))) {
            std::wcerr << L"Failed to create Command Queue!" << std::endl;
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
        swap_chain_desc.BufferCount = FRAME_COUNT;
        swap_chain_desc.Width = window_->GetWidth();
        swap_chain_desc.Height = window_->GetHeight();
        swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swap_chain1;
        if (FAILED(factory_->CreateSwapChainForHwnd(
                command_queue_.Get(),
                window_->GetHandle(),
                &swap_chain_desc,
                nullptr,
                nullptr,
                &swap_chain1))) {
            std::wcerr << L"Failed to create SwapChain!" << std::endl;
            return false;
        }

        if (FAILED(swap_chain1.As(&swap_chain_))) {
            std::wcerr << L"Failed to query SwapChain3!" << std::endl;
            return false;
        }

        if (FAILED(factory_->MakeWindowAssociation(window_->GetHandle(), DXGI_MWA_NO_ALT_ENTER))) {
            std::wcerr << L"Warning: Failed to disable Alt+Enter!" << std::endl;
        }

        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();

        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
        rtv_heap_desc.NumDescriptors = FRAME_COUNT;
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(device_->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap_)))) {
            std::wcerr << L"Failed to create RTV Descriptor Heap!" << std::endl;
            return false;
        }

        rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        render_targets_.resize(FRAME_COUNT);

        for (UINT n = 0; n < FRAME_COUNT; n++) {
            if (FAILED(swap_chain_->GetBuffer(n, IID_PPV_ARGS(&render_targets_[n])))) {
                std::wcerr << L"Failed to get SwapChain buffer " << n << L"!" << std::endl;
                return false;
            }

            device_->CreateRenderTargetView(render_targets_[n].Get(), nullptr, rtv_handle);
            rtv_handle.ptr += rtv_descriptor_size_;
        }

        for (UINT n = 0; n < FRAME_COUNT; n++) {
            if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                       IID_PPV_ARGS(&command_allocator_[n])))) {
                std::wcerr << L"Failed to create Command Allocator " << n << L"!" << std::endl;
                return false;
            }
        }

        if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator_[frame_index_].Get(),
                                              nullptr, IID_PPV_ARGS(&command_list_)))) {
            std::wcerr << L"Failed to create Command List!" << std::endl;
            return false;
        }

        command_list_->Close();

        if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))) {
            std::wcerr << L"Failed to create Fence!" << std::endl;
            return false;
        }

        fence_value_ = 1;

        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (fence_event_ == nullptr) {
            std::wcerr << L"Failed to create Fence Event!" << std::endl;
            return false;
        }

        viewport_.TopLeftX = 0.0f;
        viewport_.TopLeftY = 0.0f;
        viewport_.Width = static_cast<float>(window_->GetWidth());
        viewport_.Height = static_cast<float>(window_->GetHeight());
        viewport_.MinDepth = 0.0f;
        viewport_.MaxDepth = 1.0f;

        scissor_rect_.left = 0;
        scissor_rect_.top = 0;
        scissor_rect_.right = static_cast<LONG>(window_->GetWidth());
        scissor_rect_.bottom = static_cast<LONG>(window_->GetHeight());

        if (!CreateDepthResources()) {
            return false;
        }

        if (!CreatePhongPipeline()) {
            return false;
        }


        if (!CreateConstantBuffer()) {
            return false;
        }

        std::wcout << L"Framework initialized successfully!" << std::endl;
        return true;
    }

    void Framework::Shutdown() {
        if (fence_event_) {
            WaitForPreviousFrame();
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }

        if (constant_buffer_) {
            constant_buffer_->Unmap(0, nullptr);
            constant_buffer_mapped_ = nullptr;
        }

        constant_buffer_.Reset();
        index_buffer_.Reset();
        vertex_buffer_.Reset();
        depth_stencil_.Reset();
        pipeline_state_.Reset();
        root_signature_.Reset();
        dsv_heap_.Reset();

        render_targets_.clear();
        command_list_.Reset();

        for (UINT n = 0; n < FRAME_COUNT; n++) {
            command_allocator_[n].Reset();
        }

        rtv_heap_.Reset();
        swap_chain_.Reset();
        command_queue_.Reset();
        device_.Reset();
        factory_.Reset();

        window_ = nullptr;
    }

    void Framework::BeginFrame() {
        WaitForPreviousFrame();

        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();

        if (FAILED(command_allocator_[frame_index_]->Reset())) {
            std::wcerr << L"Failed to reset Command Allocator!" << std::endl;
            return;
        }

        if (FAILED(command_list_->Reset(command_allocator_[frame_index_].Get(), nullptr))) {
            std::wcerr << L"Failed to reset Command List!" << std::endl;
            return;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = render_targets_[frame_index_].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list_->ResourceBarrier(1, &barrier);

        command_list_->RSSetViewports(1, &viewport_);
        command_list_->RSSetScissorRects(1, &scissor_rect_);
    }

    void Framework::ClearRenderTarget(float r, float g, float b, float a) {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += frame_index_ * rtv_descriptor_size_;

        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = {};
        if (dsv_heap_) {
            dsv_handle = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
            command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);
        } else {
            command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
        }

        const float clear_color[] = {r, g, b, a};
        command_list_->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

        if (dsv_heap_) {
            command_list_->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }
    }

    void Framework::RenderCube(double total_time) {
        if (!pipeline_state_ || !root_signature_ || !vertex_buffer_ || !index_buffer_ || !constant_buffer_) {
            return;
        }

        const float t = static_cast<float>(total_time);

        SceneConstants constants = {};

        const DirectX::XMMATRIX world = DirectX::XMMatrixRotationY(t) * DirectX::XMMatrixRotationX(t * 0.5f);
        const DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 1.5f, -5.0f, 1.0f);
        const DirectX::XMVECTOR at = DirectX::XMVectorZero();
        const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, at, up);
        const float aspect = static_cast<float>(window_->GetWidth()) / static_cast<float>(window_->GetHeight());
        const DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), aspect,
                                                                         0.1f, 100.0f);

        DirectX::XMStoreFloat4x4(&constants.world, world);
        DirectX::XMStoreFloat4x4(&constants.view, view);
        DirectX::XMStoreFloat4x4(&constants.proj, proj);

        constants.light_dir_shininess = DirectX::XMFLOAT4(0.35f, 0.9f, -0.25f, 64.0f);
        constants.camera_pos = DirectX::XMFLOAT4(0.0f, 1.5f, -5.0f, 1.0f);
        constants.light_color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        constants.ambient_color = DirectX::XMFLOAT4(0.15f, 0.15f, 0.15f, 1.0f);
        constants.albedo = DirectX::XMFLOAT4(0.85f, 0.25f, 0.25f, 1.0f);

        std::memcpy(constant_buffer_mapped_, &constants, sizeof(constants));

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        command_list_->SetPipelineState(pipeline_state_.Get());
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->IASetVertexBuffers(0, 1, &vertex_buffer_view_);
        command_list_->IASetIndexBuffer(&index_buffer_view_);
        command_list_->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());
        command_list_->DrawIndexedInstanced(index_count_, 1, 0, 0, 0);
    }

    void Framework::EndFrame() {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = render_targets_[frame_index_].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list_->ResourceBarrier(1, &barrier);

        if (FAILED(command_list_->Close())) {
            std::wcerr << L"Failed to close Command List!" << std::endl;
            return;
        }

        ID3D12CommandList *command_lists[] = {command_list_.Get()};
        command_queue_->ExecuteCommandLists(static_cast<UINT>(std::size(command_lists)), command_lists);

        if (FAILED(swap_chain_->Present(1, 0))) {
            std::wcerr << L"Failed to present SwapChain!" << std::endl;
        }
    }

    void Framework::WaitForPreviousFrame() {
        const UINT64 fence = fence_value_;
        if (FAILED(command_queue_->Signal(fence_.Get(), fence))) {
            return;
        }
        fence_value_++;

        if (fence_->GetCompletedValue() < fence) {
            if (FAILED(fence_->SetEventOnCompletion(fence, fence_event_))) {
                return;
            }
            WaitForSingleObject(fence_event_, INFINITE);
        }
    }

    bool Framework::CreateDepthResources() {
        D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc = {};
        dsv_heap_desc.NumDescriptors = 1;
        dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(device_->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&dsv_heap_)))) {
            std::wcerr << L"Failed to create DSV Descriptor Heap!" << std::endl;
            return false;
        }

        D3D12_RESOURCE_DESC depth_desc = {};
        depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depth_desc.Alignment = 0;
        depth_desc.Width = window_->GetWidth();
        depth_desc.Height = window_->GetHeight();
        depth_desc.DepthOrArraySize = 1;
        depth_desc.MipLevels = 1;
        depth_desc.Format = DXGI_FORMAT_D32_FLOAT;
        depth_desc.SampleDesc.Count = 1;
        depth_desc.SampleDesc.Quality = 0;
        depth_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value = {};
        clear_value.Format = DXGI_FORMAT_D32_FLOAT;
        clear_value.DepthStencil.Depth = 1.0f;
        clear_value.DepthStencil.Stencil = 0;

        const D3D12_HEAP_PROPERTIES heap_props = HeapProperties(D3D12_HEAP_TYPE_DEFAULT);
        if (FAILED(device_->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &depth_desc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &clear_value,
                IID_PPV_ARGS(&depth_stencil_)))) {
            std::wcerr << L"Failed to create Depth Stencil resource!" << std::endl;
            return false;
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
        dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
        device_->CreateDepthStencilView(depth_stencil_.Get(), &dsv_desc,
                                        dsv_heap_->GetCPUDescriptorHandleForHeapStart());

        return true;
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

        D3D12_ROOT_PARAMETER root_param = {};
        root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        root_param.Descriptor.ShaderRegister = 0;
        root_param.Descriptor.RegisterSpace = 0;
        root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC root_desc = {};
        root_desc.NumParameters = 1;
        root_desc.pParameters = &root_param;
        root_desc.NumStaticSamplers = 0;
        root_desc.pStaticSamplers = nullptr;
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

        return true;
    }

    bool Framework::CreateConstantBuffer() {
        const UINT cb_size = Align256(sizeof(SceneConstants));

        const D3D12_HEAP_PROPERTIES heap_props = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_RESOURCE_DESC cb_desc = BufferDesc(cb_size);
        if (FAILED(device_->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &cb_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&constant_buffer_)))) {
            std::wcerr << L"Failed to create constant buffer!" << std::endl;
            return false;
        }

        void *mapped = nullptr;
        if (FAILED(constant_buffer_->Map(0, nullptr, &mapped))) {
            std::wcerr << L"Failed to map constant buffer!" << std::endl;
            return false;
        }

        constant_buffer_mapped_ = static_cast<std::uint8_t *>(mapped);
        std::memset(constant_buffer_mapped_, 0, cb_size);
        return true;
    }

    bool Framework::CreateCubeBuffers() {
        cube_mesh_ = CubeMesh::CreateUnit();
        if (cube_mesh_.GetVertexData().empty() || cube_mesh_.GetIndices().empty()) {
            return false;
        }

        MeshData cube_data = cube_mesh_.ToMeshData();

        auto cube_buffers_ = CreateMeshBuffers(cube_data);

        if (!cube_buffers_) {
            std::wcerr << L"Failed to create cube mesh buffers!" << std::endl;
            return false;
        }

        return true;
    }

    /* bool Framework::CreateCubeBuffers() {
         cube_mesh_ = CubeMesh::CreateUnit();
         if (cube_mesh_.GetVertexData().empty() || cube_mesh_.GetIndices().empty()) {
             return false;
         }

         index_count_ = static_cast<UINT>(cube_mesh_.GetIndices().size());

         const UINT vb_size = static_cast<UINT>(cube_mesh_.GetVertexData().size());
         const UINT ib_size = static_cast<UINT>(cube_mesh_.GetIndices().size() * sizeof(std::uint16_t));

         const D3D12_HEAP_PROPERTIES heap_props = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
         const D3D12_RESOURCE_DESC vb_desc = BufferDesc(vb_size);
         if (FAILED(device_->CreateCommittedResource(
                 &heap_props,
                 D3D12_HEAP_FLAG_NONE,
                 &vb_desc,
                 D3D12_RESOURCE_STATE_GENERIC_READ,
                 nullptr,
                 IID_PPV_ARGS(&vertex_buffer_)))) {
             return false;
         }

         void* vb_mapped = nullptr;
         if (FAILED(vertex_buffer_->Map(0, nullptr, &vb_mapped))) {
             return false;
         }
         std::memcpy(vb_mapped, cube_mesh_.GetVertexData().data(), vb_size);
         vertex_buffer_->Unmap(0, nullptr);

         vertex_buffer_view_.BufferLocation = vertex_buffer_->GetGPUVirtualAddress();
         vertex_buffer_view_.SizeInBytes = vb_size;
         vertex_buffer_view_.StrideInBytes = static_cast<UINT>(cube_mesh_.GetVertexStride());

         const D3D12_RESOURCE_DESC ib_desc = BufferDesc(ib_size);
         if (FAILED(device_->CreateCommittedResource(
                 &heap_props,
                 D3D12_HEAP_FLAG_NONE,
                 &ib_desc,
                 D3D12_RESOURCE_STATE_GENERIC_READ,
                 nullptr,
                 IID_PPV_ARGS(&index_buffer_)))) {
             return false;
         }

         void* ib_mapped = nullptr;
         if (FAILED(index_buffer_->Map(0, nullptr, &ib_mapped))) {
             return false;
         }
         std::memcpy(ib_mapped, cube_mesh_.GetIndices().data(), ib_size);
         index_buffer_->Unmap(0, nullptr);

         index_buffer_view_.BufferLocation = index_buffer_->GetGPUVirtualAddress();
         index_buffer_view_.SizeInBytes = ib_size;
         index_buffer_view_.Format = DXGI_FORMAT_R16_UINT;

         return true;
     }*/
    std::unique_ptr<MeshBuffers> Framework::CreateMeshBuffers(const MeshData &mesh_data) {
        if (mesh_data.vertex_data.empty()) {
            std::wcerr << L"CreateMeshBuffers: Empty vertex data!" << std::endl;
            return nullptr;
        }

        auto buffers = std::make_unique<MeshBuffers>();
        buffers->topology = mesh_data.topology;

        const UINT vb_size = static_cast<UINT>(mesh_data.vertex_data.size());
        const D3D12_HEAP_PROPERTIES heap_props = HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_RESOURCE_DESC vb_desc = BufferDesc(vb_size);

        if (FAILED(device_->CreateCommittedResource(
                &heap_props,
                D3D12_HEAP_FLAG_NONE,
                &vb_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&buffers->vertex_buffer)))) {
            std::wcerr << L"Failed to create vertex buffer!" << std::endl;
            return nullptr;
        }

        void *vb_mapped = nullptr;
        if (FAILED(buffers->vertex_buffer->Map(0, nullptr, &vb_mapped))) {
            std::wcerr << L"Failed to map vertex buffer!" << std::endl;
            return nullptr;
        }
        std::memcpy(vb_mapped, mesh_data.vertex_data.data(), vb_size);
        buffers->vertex_buffer->Unmap(0, nullptr);

        buffers->vertex_buffer_view.BufferLocation = buffers->vertex_buffer->GetGPUVirtualAddress();
        buffers->vertex_buffer_view.SizeInBytes = vb_size;
        buffers->vertex_buffer_view.StrideInBytes = mesh_data.vertex_stride;

        if (!mesh_data.indices.empty()) {
            const UINT ib_size = static_cast<UINT>(mesh_data.indices.size() * sizeof(std::uint16_t));
            const D3D12_RESOURCE_DESC ib_desc = BufferDesc(ib_size);

            if (FAILED(device_->CreateCommittedResource(
                    &heap_props,
                    D3D12_HEAP_FLAG_NONE,
                    &ib_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(&buffers->index_buffer)))) {
                std::wcerr << L"Failed to create index buffer!" << std::endl;
                return nullptr;
            }

            void *ib_mapped = nullptr;
            if (FAILED(buffers->index_buffer->Map(0, nullptr, &ib_mapped))) {
                std::wcerr << L"Failed to map index buffer!" << std::endl;
                return nullptr;
            }
            std::memcpy(ib_mapped, mesh_data.indices.data(), ib_size);
            buffers->index_buffer->Unmap(0, nullptr);

            buffers->index_buffer_view.BufferLocation = buffers->index_buffer->GetGPUVirtualAddress();
            buffers->index_buffer_view.SizeInBytes = ib_size;
            buffers->index_buffer_view.Format = DXGI_FORMAT_R16_UINT;
            buffers->index_count = static_cast<UINT>(mesh_data.indices.size());
        } else {
            buffers->index_count = mesh_data.vertex_count;
        }

        return buffers;
    }

    void Framework::RenderMesh(const MeshBuffers &buffers, const DirectX::XMMATRIX &world_matrix, double total_time) {
        if (!pipeline_state_ || !root_signature_ || !constant_buffer_) {
            return;
        }

        SceneConstants constants = {};
        DirectX::XMStoreFloat4x4(&constants.world, world_matrix);

        const DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 1.5f, -5.0f, 1.0f);
        const DirectX::XMVECTOR at = DirectX::XMVectorZero();
        const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, at, up);
        DirectX::XMStoreFloat4x4(&constants.view, view);

        const float aspect = static_cast<float>(window_->GetWidth()) / static_cast<float>(window_->GetHeight());
        const DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), aspect,
                                                                         0.1f, 100.0f);
        DirectX::XMStoreFloat4x4(&constants.proj, proj);

        constants.light_dir_shininess = DirectX::XMFLOAT4(0.35f, 0.9f, -0.25f, 64.0f);
        constants.camera_pos = DirectX::XMFLOAT4(0.0f, 1.5f, -5.0f, 1.0f);
        constants.light_color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        constants.ambient_color = DirectX::XMFLOAT4(0.15f, 0.15f, 0.15f, 1.0f);
        constants.albedo = DirectX::XMFLOAT4(0.85f, 0.25f, 0.25f, 1.0f);

        RenderMesh(buffers, constants);
    }

    void Framework::RenderMesh(const MeshBuffers &buffers, const SceneConstants &constants) {
        if (!pipeline_state_ || !root_signature_ || !constant_buffer_) {
            return;
        }

        std::memcpy(constant_buffer_mapped_, &constants, sizeof(constants));

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        command_list_->SetPipelineState(pipeline_state_.Get());
        command_list_->IASetPrimitiveTopology(buffers.topology);
        command_list_->IASetVertexBuffers(0, 1, &buffers.vertex_buffer_view);

        command_list_->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());

        if (buffers.index_buffer) {
            command_list_->IASetIndexBuffer(&buffers.index_buffer_view);
            command_list_->DrawIndexedInstanced(buffers.index_count, 1, 0, 0, 0);
        } else {
            command_list_->DrawInstanced(buffers.index_count, 1, 0, 0);
        }
    }
}
