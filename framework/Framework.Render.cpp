#include "Framework.h"

#include <iostream>
#include <iterator>

namespace gfw {
namespace {
bool g_cb_ring_overflow_logged = false;
}
    bool Framework::IsRenderReady() const {
        return pipeline_state_ && root_signature_ && constant_buffer_ && srv_heap_ && default_texture_;
    }

    void Framework::RenderMeshImpl(const MeshBuffers &buffers, const SceneConstants &constants,
                                   D3D12_GPU_DESCRIPTOR_HANDLE texture_srv, bool transparent) {
        const UINT slot_size = cb_upload_stride_ > 0u ? cb_upload_stride_ : sizeof(SceneConstants);
        if (cb_upload_cursor_ + slot_size > cb_upload_capacity_) {
            if (!g_cb_ring_overflow_logged) {
                std::cerr << "Scene constant ring buffer full; increase kRingSlots in CreateConstantBuffer.\n";
                g_cb_ring_overflow_logged = true;
            }
            return;
        }

        std::uint8_t *const slot_ptr = constant_buffer_mapped_ + cb_upload_cursor_;
        std::memcpy(slot_ptr, &constants, sizeof(constants));
        if (slot_size > sizeof(constants)) {
            std::memset(slot_ptr + sizeof(constants), 0, slot_size - sizeof(constants));
        }

        const D3D12_GPU_VIRTUAL_ADDRESS cb_gpu_va = constant_buffer_->GetGPUVirtualAddress() + cb_upload_cursor_;
        cb_upload_cursor_ += slot_size;

        ID3D12DescriptorHeap *heaps[] = {srv_heap_.Get()};
        command_list_->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        const bool rainbow_enabled = constants.effect_params.x > 0.99f && constants.effect_params.x < 1.01f;
        if (rainbow_enabled && pipeline_state_rainbow_) {
            command_list_->SetPipelineState(pipeline_state_rainbow_.Get());
        } else if (transparent && pipeline_state_transparent_) {
            command_list_->SetPipelineState(pipeline_state_transparent_.Get());
        } else {
            command_list_->SetPipelineState(pipeline_state_.Get());
        }
        command_list_->IASetPrimitiveTopology(buffers.topology);
        command_list_->IASetVertexBuffers(0, 1, &buffers.vertex_buffer_view);

        command_list_->SetGraphicsRootConstantBufferView(0, cb_gpu_va);
        command_list_->SetGraphicsRootDescriptorTable(1, texture_srv);

        if (buffers.index_buffer) {
            command_list_->IASetIndexBuffer(&buffers.index_buffer_view);
            command_list_->DrawIndexedInstanced(buffers.index_count, 1, 0, 0, 0);
        } else {
            command_list_->DrawInstanced(buffers.index_count, 1, 0, 0);
        }
    }

    void Framework::RenderMesh(const MeshBuffers &buffers, const DirectX::XMMATRIX &world_matrix, double total_time) {
        if (!IsRenderReady()) return;

        const float aspect = static_cast<float>(window_->GetWidth()) / static_cast<float>(window_->GetHeight());
        const SceneConstants constants = MakeSceneConstants(world_matrix, scene_state_, aspect,
                                                           static_cast<float>(total_time));
        RenderMesh(buffers, constants);
    }

    void Framework::RenderMesh(const MeshBuffers &buffers, const SceneConstants &constants) {
        if (!IsRenderReady()) return;
        const bool transparent = constants.albedo.w < 0.999f;
        RenderMeshImpl(buffers, constants, default_texture_->srv_gpu, transparent);
    }

    void Framework::RenderObject(const ::gfw::RenderObject &object, double total_time) {
        if (!object.mesh || !IsRenderReady()) return;

        const float aspect = static_cast<float>(window_->GetWidth()) / static_cast<float>(window_->GetHeight());
        const DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&object.world);
        SceneConstants constants = MakeSceneConstants(world, scene_state_, aspect, static_cast<float>(total_time));
        constants.albedo = object.albedo;
        constants.uv_params = object.uv_params;
        constants.effect_params = object.effect_params;

        const D3D12_GPU_DESCRIPTOR_HANDLE texture_srv =
                object.texture ? object.texture->srv_gpu : default_texture_->srv_gpu;

        const bool transparent = constants.albedo.w < 0.999f;
        RenderMeshImpl(*object.mesh, constants, texture_srv, transparent);
    }
}
