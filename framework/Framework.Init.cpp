#include "Framework.h"

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

namespace gfw {
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

        srv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        next_srv_index_ = 0;

        if (!CreateSrvHeap(64)) {
            return false;
        }

        default_texture_ = CreateSolidTexture(0xffffffffu);
        if (!default_texture_) {
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
        default_texture_.reset();
        textures_.clear();
        srv_heap_.Reset();
        depth_stencil_.Reset();
        pipeline_state_transparent_.Reset();
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
}

