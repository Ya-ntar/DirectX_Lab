#pragma once

#include "framework/Constants.h"
#include "framework/InputDevice.h"
#include "framework/Keys.h"
#include "framework/Framework.h"
#include "framework/Window.h"
#include <DirectXMath.h>
#include <algorithm>

namespace gfw {

class CameraController {
public:
    void Update(const Camera &camera, InputDevice &input, float dt,
                float speed = 2.0f, float mouse_sensitivity = 0.005f) {
        float yaw = yaw_;
        float pitch = pitch_;

        if (first_frame_) {
            DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&camera.position);
            DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&camera.target);
            DirectX::XMVECTOR f = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(t, p));
            DirectX::XMFLOAT3 f3;
            DirectX::XMStoreFloat3(&f3, f);
            float ny = std::clamp(f3.y, -1.0f, 1.0f);
            pitch = std::asin(ny);
            yaw = std::atan2(f3.x, f3.z);
            first_frame_ = false;
        }

        DirectX::SimpleMath::Vector2 mouse = input.ConsumeMouseDelta();
        yaw   += mouse.x * mouse_sensitivity;
        pitch -= mouse.y * mouse_sensitivity;
        pitch  = std::clamp(pitch, -1.5f, 1.5f);

        DirectX::XMVECTOR forward = DirectX::XMVectorSet(
            std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch), 0.0f);
        forward = DirectX::XMVector3Normalize(forward);
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));

        DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&camera.position);
        DirectX::XMVECTOR movement = DirectX::XMVectorZero();
        if (input.IsKeyDown(Keys::W) || input.IsKeyDown(Keys::Up))   movement = DirectX::XMVectorAdd(movement, forward);
        if (input.IsKeyDown(Keys::S) || input.IsKeyDown(Keys::Down)) movement = DirectX::XMVectorSubtract(movement, forward);
        if (input.IsKeyDown(Keys::A) || input.IsKeyDown(Keys::Left)) movement = DirectX::XMVectorSubtract(movement, right);
        if (input.IsKeyDown(Keys::D) || input.IsKeyDown(Keys::Right)) movement = DirectX::XMVectorAdd(movement, right);

        float lenSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(movement));
        if (lenSq > 1e-5f) {
            movement = DirectX::XMVector3Normalize(movement);
            movement = DirectX::XMVectorScale(movement, speed * dt);
            pos = DirectX::XMVectorAdd(pos, movement);
        }

        DirectX::XMVECTOR newTarget = DirectX::XMVectorAdd(pos, forward);
        DirectX::XMStoreFloat3(&camera_.position, pos);
        DirectX::XMStoreFloat3(&camera_.target, newTarget);
        camera_.up = camera.up;
        yaw_ = yaw;
        pitch_ = pitch;
    }

    const Camera &GetCamera() const { return camera_; }

private:
    Camera camera_ = {};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    bool first_frame_ = true;
};

class GameController {
public:
    void Update(Window &window, InputDevice &input, Framework &framework, float dt) {
        if (input.IsKeyDown(Keys::Escape))
            PostMessage(window.GetHandle(), WM_CLOSE, 0, 0);
        camera_.Update(framework.GetSceneState().camera, input, dt);
        framework.SetCamera(camera_.GetCamera());
    }

private:
    CameraController camera_;
};
}
