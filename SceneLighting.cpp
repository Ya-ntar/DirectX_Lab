#include "SceneLighting.h"

#include <iostream>

#include "framework/InputDevice.h"

namespace gfw {
namespace {

void Normalize3(DirectX::XMFLOAT3 &v) {
    const DirectX::XMVECTOR vec = DirectX::XMVectorSet(v.x, v.y, v.z, 0.0f);
    const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(vec);
    DirectX::XMStoreFloat3(&v, n);
}

const wchar_t *EditModeLabel(LightEditMode mode) {
    switch (mode) {
        case LightEditMode::Point:
            return L"POINT";
        case LightEditMode::Spot:
            return L"SPOT";
        case LightEditMode::Directional:
            return L"DIRECTIONAL";
    }
    return L"?";
}

} // namespace

void PrintSceneLightingHelp() {
    std::wcout << L"Light controls:\n"
               << L"  Tab - cycle edit mode: Point / Spot / Directional\n"
               << L"  +/- - increase/decrease enabled Point or Spot count\n"
               << L"  PageUp/PageDown - select active Point or Spot\n"
               << L"  IJKL + U/O - move active Point or Spot (camera-local)\n"
               << L"  Shift - faster move\n"
               << L"  Spot: R/Y + T/F/G/H - spot direction\n"
               << L"  Directional: R/Y + T/F/G/H - sun direction (world)\n";
}

void PrintTessellationAndDebugHelp() {
    std::wcout << L"\nTessellation and Debug Visualization:\n"
               << L"  T - toggle tessellation (starts ON; displacement only works with tessellation)\n"
               << L"  V - toggle wireframe mode\n"
               << L"  0 - normal lighting (exit debug mode)\n"
               << L"  1 - visualize Position buffer\n"
               << L"  2 - visualize Normal buffer\n"
               << L"  3 - visualize Albedo buffer\n";
}

void SetupDefaultLocalLights(LightControlState &state) {
    state.directional.direction = {0.65f, -0.35f, 0.67f};
    Normalize3(state.directional.direction);
    state.directional.color = {1.0f, 1.0f, 1.0f, 0.72f};
    state.directional.ambient = {0.045f, 0.048f, 0.052f, 1.0f};

    state.point_lights = {
        {{0.0f, 4.5f, -5.0f}, 42.0f, {1.0f, 1.0f, 1.0f}, 1.35f},
        {{-8.0f, 3.0f, -1.0f}, 14.0f, {1.0f, 1.0f, 1.0f}, 0.75f},
        {{-2.0f, 2.2f, 6.0f}, 12.0f, {1.0f, 1.0f, 1.0f}, 0.65f},
        {{6.0f, 2.8f, -5.0f}, 12.0f, {1.0f, 1.0f, 1.0f}, 0.7f},
        {{11.0f, 3.5f, 3.0f}, 14.0f, {1.0f, 1.0f, 1.0f}, 0.72f},
    };
    state.spot_lights = {
        {{0.0f, 8.0f, 0.0f}, 25.0f, {0.0f, -1.0f, 0.0f}, 0.88f, {1.0f, 1.0f, 1.0f}, 1.1f},
        {{-10.0f, 5.0f, -10.0f}, 22.0f, {0.6f, -0.6f, 0.6f}, 0.90f, {1.0f, 1.0f, 1.0f}, 1.0f},
    };
    state.enabled_point_count = state.point_lights.size();
    state.enabled_spot_count = state.spot_lights.size();
}

void ApplyLightControls(InputDevice &input, const Camera &camera, float dt, LightControlState &state) {
    auto pressed_once = [&](Keys key) -> bool {
        const int code = static_cast<int>(key);
        const bool down = input.IsKeyDown(key);
        const bool was_down = state.key_latch[code];
        state.key_latch[code] = down;
        return down && !was_down;
    };

    if (pressed_once(Keys::Tab)) {
        switch (state.edit_mode) {
            case LightEditMode::Point:
                state.edit_mode = LightEditMode::Spot;
                break;
            case LightEditMode::Spot:
                state.edit_mode = LightEditMode::Directional;
                break;
            case LightEditMode::Directional:
                state.edit_mode = LightEditMode::Point;
                break;
        }
        std::wcout << L"[Light] Mode: " << EditModeLabel(state.edit_mode) << L"\n";
    }

    if (state.edit_mode == LightEditMode::Point || state.edit_mode == LightEditMode::Spot) {
        if (pressed_once(Keys::OemPlus)) {
            if (state.edit_mode == LightEditMode::Spot) {
                const size_t next = state.enabled_spot_count + 1;
                state.enabled_spot_count = (next < state.spot_lights.size()) ? next : state.spot_lights.size();
            } else {
                const size_t next = state.enabled_point_count + 1;
                state.enabled_point_count = (next < state.point_lights.size()) ? next : state.point_lights.size();
            }
        }
        if (pressed_once(Keys::OemMinus)) {
            if (state.edit_mode == LightEditMode::Spot) {
                if (state.enabled_spot_count > 0) {
                    state.enabled_spot_count--;
                }
            } else {
                if (state.enabled_point_count > 0) {
                    state.enabled_point_count--;
                }
            }
        }

        if (pressed_once(Keys::PageUp)) {
            if (state.edit_mode == LightEditMode::Spot && !state.spot_lights.empty()) {
                state.active_spot = (state.active_spot + 1) % state.spot_lights.size();
            } else if (state.edit_mode == LightEditMode::Point && !state.point_lights.empty()) {
                state.active_point = (state.active_point + 1) % state.point_lights.size();
            }
        }
        if (pressed_once(Keys::PageDown)) {
            if (state.edit_mode == LightEditMode::Spot && !state.spot_lights.empty()) {
                state.active_spot = (state.active_spot + state.spot_lights.size() - 1) % state.spot_lights.size();
            } else if (state.edit_mode == LightEditMode::Point && !state.point_lights.empty()) {
                state.active_point = (state.active_point + state.point_lights.size() - 1) % state.point_lights.size();
            }
        }
    }

    const float speed_mul =
        input.IsKeyDown(Keys::LeftShift) || input.IsKeyDown(Keys::RightShift) ? 3.0f : 1.0f;
    const float move_step = state.move_speed * speed_mul * dt;

    const DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&camera.position);
    const DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&camera.target);
    DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, eye));
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));

    DirectX::XMVECTOR move = DirectX::XMVectorZero();
    if (input.IsKeyDown(Keys::J)) {
        move = DirectX::XMVectorSubtract(move, right);
    }
    if (input.IsKeyDown(Keys::L)) {
        move = DirectX::XMVectorAdd(move, right);
    }
    if (input.IsKeyDown(Keys::I)) {
        move = DirectX::XMVectorAdd(move, forward);
    }
    if (input.IsKeyDown(Keys::K)) {
        move = DirectX::XMVectorSubtract(move, forward);
    }
    if (input.IsKeyDown(Keys::U)) {
        move = DirectX::XMVectorAdd(move, up);
    }
    if (input.IsKeyDown(Keys::O)) {
        move = DirectX::XMVectorSubtract(move, up);
    }

    if ((state.edit_mode == LightEditMode::Point || state.edit_mode == LightEditMode::Spot) &&
        DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(move)) > 1e-5f) {
        move = DirectX::XMVectorScale(DirectX::XMVector3Normalize(move), move_step);
        DirectX::XMFLOAT3 delta = {};
        DirectX::XMStoreFloat3(&delta, move);

        if (state.edit_mode == LightEditMode::Spot && !state.spot_lights.empty()) {
            SpotLight &active = state.spot_lights[state.active_spot];
            active.position.x += delta.x;
            active.position.y += delta.y;
            active.position.z += delta.z;
        } else if (state.edit_mode == LightEditMode::Point && !state.point_lights.empty()) {
            PointLight &active = state.point_lights[state.active_point];
            active.position.x += delta.x;
            active.position.y += delta.y;
            active.position.z += delta.z;
        }
    }

    if (state.edit_mode == LightEditMode::Spot && !state.spot_lights.empty()) {
        SpotLight &active = state.spot_lights[state.active_spot];
        if (input.IsKeyDown(Keys::F)) {
            active.direction.x -= dt;
        }
        if (input.IsKeyDown(Keys::H)) {
            active.direction.x += dt;
        }
        if (input.IsKeyDown(Keys::T)) {
            active.direction.z += dt;
        }
        if (input.IsKeyDown(Keys::G)) {
            active.direction.z -= dt;
        }
        if (input.IsKeyDown(Keys::R)) {
            active.direction.y += dt;
        }
        if (input.IsKeyDown(Keys::Y)) {
            active.direction.y -= dt;
        }
        Normalize3(active.direction);
    }

    if (state.edit_mode == LightEditMode::Directional) {
        DirectX::XMFLOAT3 &dir = state.directional.direction;
        if (input.IsKeyDown(Keys::F)) {
            dir.x -= dt;
        }
        if (input.IsKeyDown(Keys::H)) {
            dir.x += dt;
        }
        if (input.IsKeyDown(Keys::T)) {
            dir.z += dt;
        }
        if (input.IsKeyDown(Keys::G)) {
            dir.z -= dt;
        }
        if (input.IsKeyDown(Keys::R)) {
            dir.y += dt;
        }
        if (input.IsKeyDown(Keys::Y)) {
            dir.y -= dt;
        }
        Normalize3(dir);
    }
}

} // namespace gfw
