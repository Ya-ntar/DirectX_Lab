#include "CubeMesh.h"
#include <array>
#include <cstring>

namespace gfw {
    CubeMesh CubeMesh::CreateUnit() {
        CubeMesh mesh;

        struct Vertex {
            float px, py, pz;
            float nx, ny, nz;
        };

        const std::array<Vertex, 24> vertices = {
                Vertex{-1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f},
                Vertex{-1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f},
                Vertex{1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f},
                Vertex{1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f},

                Vertex{-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                Vertex{1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                Vertex{1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
                Vertex{-1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},

                Vertex{-1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f},
                Vertex{-1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f},
                Vertex{1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f},
                Vertex{1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f},

                Vertex{-1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f},
                Vertex{1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f},
                Vertex{1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f},
                Vertex{-1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f},

                Vertex{-1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f},
                Vertex{-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f},
                Vertex{-1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f},
                Vertex{-1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f},

                Vertex{1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f},
                Vertex{1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f},
                Vertex{1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
                Vertex{1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f},
        };

        const std::array<std::uint16_t, 36> indices = {
                0, 1, 2, 0, 2, 3,
                4, 5, 6, 4, 6, 7,
                8, 9, 10, 8, 10, 11,
                12, 13, 14, 12, 14, 15,
                16, 17, 18, 16, 18, 19,
                20, 21, 22, 20, 22, 23,
        };

        mesh.vertex_stride_ = static_cast<std::uint32_t>(sizeof(Vertex));
        mesh.vertex_data_.resize(sizeof(vertices));
        std::memcpy(mesh.vertex_data_.data(), vertices.data(), sizeof(vertices));
        mesh.indices_.assign(indices.begin(), indices.end());
        return mesh;
    }
}
