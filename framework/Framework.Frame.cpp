#include "Framework.h"
#include "FrameworkInternal.h"

#include <iterator>

namespace gfw {
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

        const auto barrier = detail::TransitionBarrier(
                render_targets_[frame_index_].Get(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
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

    void Framework::EndFrame() {
        const auto barrier = detail::TransitionBarrier(
                render_targets_[frame_index_].Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT);
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
}

