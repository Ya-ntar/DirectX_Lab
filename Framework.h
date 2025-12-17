#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include "Exports.h"
#include "Window.h"

using Microsoft::WRL::ComPtr;

namespace gfw {
    class GAMEFRAMEWORK_API Framework {
    private:
        static constexpr UINT FRAME_COUNT = 2;

        Window *window_ = nullptr;

        ComPtr<IDXGIFactory4> factory_;
        ComPtr<ID3D12Device> device_;
        ComPtr<ID3D12CommandQueue> command_queue_;
        ComPtr<IDXGISwapChain3> swap_chain_;
        ComPtr<ID3D12DescriptorHeap> rtv_heap_;
        ComPtr<ID3D12CommandAllocator> command_allocator_[FRAME_COUNT];
        ComPtr<ID3D12GraphicsCommandList> command_list_;
        ComPtr<ID3D12Fence> fence_;

        UINT rtv_descriptor_size_ = 0;
        UINT frame_index_ = 0;
        UINT64 fence_value_ = 0;
        HANDLE fence_event_ = nullptr;

        std::vector<ComPtr<ID3D12Resource>> render_targets_;

        void WaitForPreviousFrame();

    public:
        Framework();

        ~Framework();

        Framework(const Framework &) = delete;

        Framework &operator=(const Framework &) = delete;

        Framework(Framework &&) = delete;

        Framework &operator=(Framework &&) = delete;

        bool Initialize(Window *window);

        void Shutdown();

        void BeginFrame();

        void ClearRenderTarget(float r, float g, float b, float a);

        void EndFrame();

        bool IsInitialized() const { return device_.Get() != nullptr; }
    };
}
