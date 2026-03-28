#pragma once

#include <vector>
#include <string>
#include "ControlSettings.h"
#include <DirectXMath.h>

namespace gfw {

enum class MaterialMode {
    Texture,
    SolidColor,
    Rainbow
};

struct CameraConfig {
    DirectX::XMFLOAT3 position = {0.0f, 1.5f, -5.0f};
    DirectX::XMFLOAT3 target = {0.0f, 1.0f, 0.0f};
};

struct SceneObjectConfig {
    std::wstring name;
    std::wstring obj_path;
    std::wstring mtl_path;
    std::wstring texture_path;
    MaterialMode material_mode = MaterialMode::Texture;
    DirectX::XMFLOAT4 solid_color = {1.0f, 1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT3 position = {0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 scale = {1.0f, 1.0f, 1.0f};
    float rainbow_speed = 1.0f;
};

struct RenderSettings {
    // Tessellation parameters
    float displacement_scale = 0.05f;
    float normal_displacement_scale = 0.11f;
    float tessellation_min_level = 2.0f;
    float tessellation_max_level = 8.0f;
    bool tessellation_enabled = true;

    // UV mapping parameters
    DirectX::XMFLOAT2 uv_scale = {2.0f, 2.0f};
    DirectX::XMFLOAT2 uv_offset = {0.08f, -0.05f};
};

struct AppConfig {
    CameraConfig camera = {};
    ControlSettings controls = {};
    RenderSettings render_settings = {};
    std::vector<SceneObjectConfig> objects = {};
};

void AddObjectsToConfig(AppConfig &config);

} // namespace gfw

