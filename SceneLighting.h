#pragma once

#include <unordered_map>
#include <vector>

#include <DirectXMath.h>

#include "framework/Constants.h"
#include "framework/Keys.h"

namespace gfw {

class InputDevice;
class RenderingSystem;

struct PointLight {
    DirectX::XMFLOAT3 position = {0.0f, 1.0f, 0.0f};
    float range = 7.0f;
    DirectX::XMFLOAT3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

struct SpotLight {
    DirectX::XMFLOAT3 position = {0.0f, 2.0f, 0.0f};
    float range = 15.0f;
    DirectX::XMFLOAT3 direction = {0.0f, -1.0f, 0.0f};
    float angle_cos = 0.85f;
    DirectX::XMFLOAT3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 2.0f;
};

enum class LightEditMode {
    Point,
    Spot,
    Directional,
};

struct LightControlState {
    DirectionalLight directional = {};
    std::vector<PointLight> point_lights;
    std::vector<SpotLight> spot_lights;
    size_t active_point = 0;
    size_t active_spot = 0;
    size_t enabled_point_count = 0;
    size_t enabled_spot_count = 0;
    float move_speed = 5.0f;
    LightEditMode edit_mode = LightEditMode::Point;
    std::unordered_map<int, bool> key_latch;
};

void PrintSceneLightingHelp();

void PrintTessellationAndDebugHelp();

void SetupDefaultLocalLights(LightControlState &state);

void ApplyLightControls(InputDevice &input, const Camera &camera, float dt, LightControlState &state);

void PushLightsToRenderingSystem(const LightControlState &state, RenderingSystem &rendering);

} // namespace gfw
