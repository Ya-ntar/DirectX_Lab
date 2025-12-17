#include "Framework.h"
#include <iostream>
#include <iterator>

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
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
            {
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

        std::wcout << L"Framework initialized successfully!" << std::endl;
        return true;
    }

    void Framework::Shutdown() {
        if (fence_event_) {
            WaitForPreviousFrame();
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }

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
    }

    void Framework::ClearRenderTarget(float r, float g, float b, float a) {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr += frame_index_ * rtv_descriptor_size_;

        command_list_->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

        const float clear_color[] = {r, g, b, a};
        command_list_->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
    }

    void Framework::EndFrame() {
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
}
