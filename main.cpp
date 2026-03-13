#include <windows.h>
#include <iostream>
#include "framework/Window.h"
#include "framework/InputDevice.h"
#include "framework/Keys.h"
#include "framework/Framework.h"
#include "framework/Timer.h"
#include "CubeMesh.h"
#include "MeshData.h"
#include "MeshLoader.h"
#include <memory>
#include <cmath>
#include <algorithm>

using namespace gfw;

int main() {
    Window window;
    Window::WindowDesc desc;
    desc.title = L"DirectX 12 Window";
    desc.width = 1280;
    desc.height = 720;
    desc.instance = GetModuleHandle(nullptr);

    if (!window.Create(desc)) {
        std::wcerr << L"Failed to create window!" << std::endl;
        return -1;
    }

    try {
        InputDevice input_device(window.GetHandle());
        window.SetInputDevice(&input_device);

        Framework framework;
        if (!framework.Initialize(&window)) {
            std::wcerr << L"Failed to initialize Framework!" << std::endl;
            return -1;
        }

        MeshData meshData = MeshLoader::LoadObj(L"sponza.obj");
        if (meshData.vertex_count == 0) {
            std::wcerr << L"Sponza not found or empty, using cube." << std::endl;
            meshData = CubeMesh::CreateUnit().ToMeshData();
        }
        std::unique_ptr<MeshBuffers> meshBuffers = framework.CreateMeshBuffers(meshData);
        if (!meshBuffers) {
            std::wcerr << L"Failed to create mesh buffers!" << std::endl;
            return -1;
        }

        Timer timer;
        timer.Reset();

        std::wcout << L"Window created successfully. Size: " << window.GetWidth()
                   << L"x" << window.GetHeight() << std::endl;
        std::wcout << L"DirectX 12 initialized. Press ESC to exit." << std::endl;

        float yaw = 0.0f;
        float pitch = 0.0f;
        bool first_frame = true;

        while (window.IsRunning()) {
            window.ProcessMessages();

            timer.Tick();

            const float dt = static_cast<float>(timer.GetDeltaTime());
            const float speed = 2.0f;
            const float mouseSensitivity = 0.005f;

            auto camera = framework.GetSceneState().camera;
            
            auto mouseOffset = input_device.GetMouseOffset();
            if (mouseOffset.x != 0 || mouseOffset.y != 0) {
                yaw += mouseOffset.x * mouseSensitivity;
                pitch += mouseOffset.y * mouseSensitivity;
                pitch = std::clamp(pitch, -1.5f, 1.5f);
            }

            // Sync yaw/pitch from camera on first frame
            if (first_frame) {
                DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&camera.position);
                DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&camera.target);
                DirectX::XMVECTOR f = DirectX::XMVectorSubtract(t, p);
                f = DirectX::XMVector3Normalize(f);
                
                DirectX::XMFLOAT3 f_float;
                DirectX::XMStoreFloat3(&f_float, f);
                
                pitch = std::asin(f_float.y);
                yaw = std::atan2(f_float.x, f_float.z);
                first_frame = false;
            }

            DirectX::XMVECTOR forwardVec = DirectX::XMVectorSet(
                std::sin(yaw) * std::cos(pitch),
                std::sin(pitch),
                std::cos(yaw) * std::cos(pitch),
                0.0f
            );
            forwardVec = DirectX::XMVector3Normalize(forwardVec);
            
            DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            DirectX::XMVECTOR right = DirectX::XMVector3Cross(up, forwardVec);
            right = DirectX::XMVector3Normalize(right);

            DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&camera.position);
            
            DirectX::XMVECTOR movement = DirectX::XMVectorZero();

            if (input_device.IsKeyDown(Keys::W) || input_device.IsKeyDown(Keys::Up)) {
                movement = DirectX::XMVectorAdd(movement, forwardVec);
            }
            if (input_device.IsKeyDown(Keys::S) || input_device.IsKeyDown(Keys::Down)) {
                movement = DirectX::XMVectorSubtract(movement, forwardVec);
            }
            if (input_device.IsKeyDown(Keys::A) || input_device.IsKeyDown(Keys::Left)) {
                movement = DirectX::XMVectorSubtract(movement, right);
            }
            if (input_device.IsKeyDown(Keys::D) || input_device.IsKeyDown(Keys::Right)) {
                movement = DirectX::XMVectorAdd(movement, right);
            }

            if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(movement)) > 0.00001f) {
                movement = DirectX::XMVector3Normalize(movement);
                movement = DirectX::XMVectorScale(movement, speed * dt);

                pos = DirectX::XMVectorAdd(pos, movement);
            }
            
            DirectX::XMVECTOR newTarget = DirectX::XMVectorAdd(pos, forwardVec);
            
            DirectX::XMStoreFloat3(&camera.position, pos);
            DirectX::XMStoreFloat3(&camera.target, newTarget);
            framework.SetCamera(camera);

            framework.BeginFrame();

            framework.ClearRenderTarget(0.39f, 0.58f, 0.93f, 1.0f);

            DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
            framework.RenderMesh(*meshBuffers, world, timer.GetTotalTime());

            framework.EndFrame();
        }

        framework.Shutdown();
        return 0;
    }
    catch (const std::exception &e) {
        std::wcerr << L"Error: " << e.what() << std::endl;
        return -1;
    }
}
