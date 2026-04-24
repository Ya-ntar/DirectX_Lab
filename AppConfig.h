#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>

#include "ControlSettings.h"
#include "framework/Constants.h"

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
    float vertex_wave_amplitude = 0.0f;
    float vertex_wave_frequency = 2.0f;
    // Material filter: if not empty, only submeshes with these material names will be loaded
    std::vector<std::wstring> material_filter = {};
};

struct AppConfig {
    CameraConfig camera = {};
    ControlSettings controls = {};
    std::vector<SceneObjectConfig> objects = {};
};

AppConfig BuildAppConfig();
Camera BuildInitialCamera(const AppConfig &config);

// Debug function to print all materials in a model
void PrintModelMaterials(const std::wstring &obj_path, const std::wstring &mtl_path);

}  // namespace gfw

