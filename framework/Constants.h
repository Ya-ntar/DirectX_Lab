
#pragma once

#include <DirectXMath.h>

namespace gfw {

    struct SceneConstants {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
        DirectX::XMFLOAT4 light_dir_shininess;
        DirectX::XMFLOAT4 camera_pos;
        DirectX::XMFLOAT4 light_color;
        DirectX::XMFLOAT4 ambient_color;
        DirectX::XMFLOAT4 albedo;
        float time_seconds = 0.0f;
        DirectX::XMFLOAT3 _padding0 = {0.0f, 0.0f, 0.0f};
    };

    struct Camera {
        DirectX::XMFLOAT3 position = {0.0f, 1.5f, -5.0f};
        DirectX::XMFLOAT3 target = {0.0f, 0.0f, 0.0f};
        DirectX::XMFLOAT3 up = {0.0f, 1.0f, 0.0f};

        [[nodiscard]] DirectX::XMMATRIX ViewMatrix() const {
            const DirectX::XMVECTOR eye_v = DirectX::XMLoadFloat3(&position);
            const DirectX::XMVECTOR at_v = DirectX::XMLoadFloat3(&target);
            const DirectX::XMVECTOR up_v = DirectX::XMLoadFloat3(&up);
            return DirectX::XMMatrixLookAtLH(eye_v, at_v, up_v);
        }

        [[nodiscard]] DirectX::XMFLOAT4 Position4(float w = 1.0f) const {
            return {position.x, position.y, position.z, w};
        }
    };

    struct PerspectiveProjection {
        float fov_y_degrees = 60.0f;
        float near_z = 0.1f;
        float far_z = 100.0f;

        [[nodiscard]] DirectX::XMMATRIX Matrix(float aspect) const {
            return DirectX::XMMatrixPerspectiveFovLH(
                    DirectX::XMConvertToRadians(fov_y_degrees),
                    aspect,
                    near_z,
                    far_z);
        }
    };

    struct DirectionalLight {
        DirectX::XMFLOAT3 direction = {0.35f, 0.9f, -0.25f};
        float shininess = 64.0f;
        DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        DirectX::XMFLOAT4 ambient = {0.15f, 0.15f, 0.15f, 1.0f};

        [[nodiscard]] DirectX::XMFLOAT4 DirShininess() const {
            return {direction.x, direction.y, direction.z, shininess};
        }
    };

    struct PhongMaterial {
        DirectX::XMFLOAT4 albedo = {0.85f, 0.25f, 0.25f, 1.0f};
    };

    struct SceneState {
        Camera camera = {};
        PerspectiveProjection projection = {};
        DirectionalLight light = {};
        PhongMaterial material = {};
    };

    [[nodiscard]] inline SceneConstants MakeSceneConstants(
            const DirectX::XMMATRIX &world,
            const SceneState &scene,
            float aspect,
            float time_seconds) {
        SceneConstants constants = {};

        DirectX::XMStoreFloat4x4(&constants.world, world);
        DirectX::XMStoreFloat4x4(&constants.view, scene.camera.ViewMatrix());
        DirectX::XMStoreFloat4x4(&constants.proj, scene.projection.Matrix(aspect));

        constants.light_dir_shininess = scene.light.DirShininess();
        constants.camera_pos = scene.camera.Position4(1.0f);
        constants.light_color = scene.light.color;
        constants.ambient_color = scene.light.ambient;
        constants.albedo = scene.material.albedo;
        constants.time_seconds = time_seconds;

        return constants;
    }
}
