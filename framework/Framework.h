#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <memory>
#include "Exports.h"
#include "Window.h"
#include "../MeshData.h"
#include "../CubeMesh.h"
#include "Constants.h"

using Microsoft::WRL::ComPtr;

namespace gfw {
    struct Texture2D {
        ComPtr<ID3D12Resource> resource;
        D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu = {};
    };

    struct RenderObject {
        const MeshBuffers *mesh = nullptr;
        DirectX::XMFLOAT4X4 world = {};
        DirectX::XMFLOAT4 albedo = {0.85f, 0.25f, 0.25f, 1.0f};
        std::shared_ptr<Texture2D> texture = {};

        RenderObject() {
            DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixIdentity());
        }
    };

    class GAMEFRAMEWORK_API Framework {
    private:
        static constexpr UINT FRAME_COUNT = 2;

        Window *window_ = nullptr;

        ComPtr<IDXGIFactory4> factory_;
        ComPtr<ID3D12Device> device_;
        ComPtr<ID3D12CommandQueue> command_queue_;
        ComPtr<IDXGISwapChain3> swap_chain_;
        ComPtr<ID3D12DescriptorHeap> rtv_heap_;
        ComPtr<ID3D12DescriptorHeap> dsv_heap_;
        ComPtr<ID3D12DescriptorHeap> srv_heap_;
        ComPtr<ID3D12CommandAllocator> command_allocator_[FRAME_COUNT];
        ComPtr<ID3D12GraphicsCommandList> command_list_;
        ComPtr<ID3D12Fence> fence_;

        ComPtr<ID3D12RootSignature> root_signature_;
        ComPtr<ID3D12PipelineState> pipeline_state_;
        ComPtr<ID3D12PipelineState> pipeline_state_transparent_;

        ComPtr<ID3D12Resource> depth_stencil_;

        CubeMesh cube_mesh_;
        ComPtr<ID3D12Resource> vertex_buffer_;
        ComPtr<ID3D12Resource> index_buffer_;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view_{};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view_{};
        UINT index_count_ = 0;

        ComPtr<ID3D12Resource> constant_buffer_;
        std::uint8_t *constant_buffer_mapped_ = nullptr;

        SceneState scene_state_ = {};
        std::vector<std::shared_ptr<Texture2D>> textures_;
        std::shared_ptr<Texture2D> default_texture_;
        UINT srv_descriptor_size_ = 0;
        UINT next_srv_index_ = 0;

        D3D12_VIEWPORT viewport_{};
        D3D12_RECT scissor_rect_{};

        UINT rtv_descriptor_size_ = 0;
        UINT frame_index_ = 0;
        UINT64 fence_value_ = 0;
        HANDLE fence_event_ = nullptr;

        std::vector<ComPtr<ID3D12Resource>> render_targets_;

        void WaitForPreviousFrame();

        bool CreateDepthResources();

        bool CreatePhongPipeline();

        bool CreateCubeBuffers();

        bool CreateConstantBuffer();

        bool CreateSrvHeap(UINT descriptor_count);

        std::shared_ptr<Texture2D> CreateSolidTexture(std::uint32_t rgba8);

        void RenderMeshImpl(const MeshBuffers &buffers, const SceneConstants &constants,
                            D3D12_GPU_DESCRIPTOR_HANDLE texture_srv, bool transparent);

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

        void RenderCube(double total_time);

        void EndFrame();

        [[nodiscard]] bool IsInitialized() const { return device_.Get() != nullptr; }

        std::unique_ptr<MeshBuffers> CreateMeshBuffers(const MeshData &mesh_data);

        std::shared_ptr<Texture2D> CreateSolidTexture(const DirectX::XMFLOAT4 &color);

        void RenderMesh(const MeshBuffers &buffers, const DirectX::XMMATRIX &world_matrix, double total_time);

        void RenderMesh(const MeshBuffers &buffers, const SceneConstants &constants);

        void RenderObject(const ::gfw::RenderObject &object, double total_time);

        [[nodiscard]] const SceneState &GetSceneState() const { return scene_state_; }

        void SetCamera(const Camera& camera) { scene_state_.camera = camera; }

    };
}
