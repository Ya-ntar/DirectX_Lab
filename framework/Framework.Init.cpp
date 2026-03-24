#include "Framework.h"
#include "FrameworkInternal.h"

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

        HRESULT co_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(co_hr)) {
            com_initialized_ = true;
        } else if (co_hr != RPC_E_CHANGED_MODE) {
            std::wcerr << L"Failed to initialize COM for texture loading!" << std::endl;
            return false;
        }


        if (!device_manager_.Initialize()) {
            return false;
        }

        factory_ = device_manager_.GetFactory();
        device_ = device_manager_.GetDevice();

        D3D12_COMMAND_QUEUE_DESC queue_desc = {};
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        if (detail::CheckFailed(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue_)), L"Failed to create Command Queue!"))
            return false;

        DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
        swap_chain_desc.BufferCount = FRAME_COUNT;
        swap_chain_desc.Width = window_->GetWidth();
        swap_chain_desc.Height = window_->GetHeight();
        swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swap_chain_desc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swap_chain1;
        if (detail::CheckFailed(factory_->CreateSwapChainForHwnd(
                command_queue_.Get(), window_->GetHandle(), &swap_chain_desc, nullptr, nullptr, &swap_chain1), L"Failed to create SwapChain!"))
            return false;
        if (detail::CheckFailed(swap_chain1.As(&swap_chain_), L"Failed to query SwapChain3!"))
            return false;

        if (FAILED(factory_->MakeWindowAssociation(window_->GetHandle(), DXGI_MWA_NO_ALT_ENTER))) {
            std::wcerr << L"Warning: Failed to disable Alt+Enter!" << std::endl;
        }

        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();

        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
        rtv_heap_desc.NumDescriptors = FRAME_COUNT;
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (detail::CheckFailed(device_->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap_)), L"Failed to create RTV Descriptor Heap!"))
            return false;

        rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        render_targets_.resize(FRAME_COUNT);

        for (UINT n = 0; n < FRAME_COUNT; n++) {
            if (detail::CheckFailed(swap_chain_->GetBuffer(n, IID_PPV_ARGS(&render_targets_[n])), L"Failed to get SwapChain buffer!"))
                return false;
            device_->CreateRenderTargetView(render_targets_[n].Get(), nullptr, rtv_handle);
            rtv_handle.ptr += rtv_descriptor_size_;
        }

        for (UINT n = 0; n < FRAME_COUNT; n++) {
            if (detail::CheckFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator_[n])), L"Failed to create Command Allocator!"))
                return false;
        }
        if (detail::CheckFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator_[frame_index_].Get(), nullptr, IID_PPV_ARGS(&command_list_)), L"Failed to create Command List!"))
            return false;
        command_list_->Close();

        if (detail::CheckFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), L"Failed to create Fence!"))
            return false;
        fence_value_ = 1;
        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!fence_event_) {
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
        default_texture_.reset();
        textures_.clear();
        srv_heap_.Reset();
        depth_stencil_.Reset();
        pipeline_state_rainbow_.Reset();
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
        if (com_initialized_) {
            CoUninitialize();
            com_initialized_ = false;
        }

        window_ = nullptr;
    }
}

