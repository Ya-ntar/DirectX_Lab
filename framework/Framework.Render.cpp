#include "Framework.h"

#include <iterator>

namespace gfw {
    bool Framework::IsRenderReady() const {
        return pipeline_state_ && root_signature_ && constant_buffer_ && srv_heap_ && default_texture_;
    }

    void Framework::RenderMeshImpl(const MeshBuffers &buffers, const SceneConstants &constants,
                                   D3D12_GPU_DESCRIPTOR_HANDLE texture_srv, bool transparent) {
        std::memcpy(constant_buffer_mapped_, &constants, sizeof(constants));

        ID3D12DescriptorHeap *heaps[] = {srv_heap_.Get()};
        command_list_->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        if (transparent && pipeline_state_transparent_) {
            command_list_->SetPipelineState(pipeline_state_transparent_.Get());
        } else {
            command_list_->SetPipelineState(pipeline_state_.Get());
        }
        command_list_->IASetPrimitiveTopology(buffers.topology);
        command_list_->IASetVertexBuffers(0, 1, &buffers.vertex_buffer_view);

        command_list_->SetGraphicsRootConstantBufferView(0, constant_buffer_->GetGPUVirtualAddress());
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

        const D3D12_GPU_DESCRIPTOR_HANDLE texture_srv =
                object.texture ? object.texture->srv_gpu : default_texture_->srv_gpu;

        const bool transparent = constants.albedo.w < 0.999f;
        RenderMeshImpl(*object.mesh, constants, texture_srv, transparent);
    }
}
