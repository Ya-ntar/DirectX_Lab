#include "PlaneMesh.h"

#include <array>

namespace gfw {
    PlaneMesh PlaneMesh::CreateUnit() {
        PlaneMesh mesh;

        // Vertex layout: Position (3 floats), Normal (3 floats), UV (2 floats)
        mesh.vertex_stride_ = 8 * sizeof(float);

        // Create a plane quad
        struct Vertex {
            float position[3];
            float normal[3];
            float uv[2];
        };

        std::array<Vertex, 4> vertices = {{
            // Bottom-left
            {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
            // Bottom-right
            {{0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
            // Top-right
            {{0.5f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            // Top-left
            {{-0.5f, 0.0f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}
        }};

        mesh.vertex_data_.resize(vertices.size() * sizeof(Vertex));
        std::memcpy(mesh.vertex_data_.data(), vertices.data(), mesh.vertex_data_.size());

        // Indices for two triangles
        mesh.indices_ = {0, 1, 2, 0, 2, 3};

        return mesh;
    }
}
