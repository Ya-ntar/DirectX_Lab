#pragma once

#include "MeshData.h"
#include <cstdint>
#include <vector>

namespace gfw {
    class CubeMesh {
    public:
        static CubeMesh CreateUnit();

        [[nodiscard]] MeshData ToMeshData() const {
            MeshData data;
            data.vertex_data = vertex_data_;
            data.vertex_stride = vertex_stride_;
            data.vertex_count = static_cast<UINT>(vertex_data_.size()) / vertex_stride_;
            data.indices.assign(indices_.begin(), indices_.end());
            data.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            return data;
        }

        [[nodiscard]] const std::vector<std::uint8_t>& GetVertexData() const { return vertex_data_; }
        std::uint32_t GetVertexStride() const { return vertex_stride_; }
        [[nodiscard]] const std::vector<std::uint32_t>& GetIndices() const { return indices_; }

    private:
        std::vector<std::uint8_t> vertex_data_;
        std::uint32_t vertex_stride_ = 0;
        std::vector<std::uint32_t> indices_;
    };
}