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

struct AppConfig {
    CameraConfig camera = {};
    ControlSettings controls = {};
    std::vector<SceneObjectConfig> objects = {};
};

void AddObjectsToConfig(AppConfig &config);

} // namespace gfw

