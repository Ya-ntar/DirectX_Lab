#pragma once

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace gfw {
    struct MeshData {
        std::vector<std::uint8_t> vertex_data;
        std::uint32_t vertex_stride = 0;
        std::uint32_t vertex_count = 0;
        std::vector<std::uint32_t> indices;
        D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    };

    class MeshBuffers {
    public:
        MeshBuffers() = default;

        ~MeshBuffers() = default;

        MeshBuffers(const MeshBuffers &) = delete;

        MeshBuffers &operator=(const MeshBuffers &) = delete;

        MeshBuffers(MeshBuffers &&) = default;

        MeshBuffers &operator=(MeshBuffers &&) = default;

        ComPtr<ID3D12Resource> vertex_buffer;
        ComPtr<ID3D12Resource> index_buffer;
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view = {};
        D3D12_INDEX_BUFFER_VIEW index_buffer_view = {};
        UINT index_count = 0;
        D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    };
}