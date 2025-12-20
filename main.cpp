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

        // Load Sponza
        MeshData sponzaData = MeshLoader::LoadObj(L"sponza.obj");
        std::unique_ptr<MeshBuffers> meshBuffers;
        
        if (sponzaData.vertex_count > 0) {
            meshBuffers = framework.CreateMeshBuffers(sponzaData);
            if (!meshBuffers) {
                 std::wcerr << L"Failed to create mesh buffers for Sponza!" << std::endl;
                 return -1;
            }
        } else {
             std::wcerr << L"Failed to load Sponza or file is empty. Falling back to Cube." << std::endl;
             CubeMesh cube_mesh = CubeMesh::CreateUnit();
             meshBuffers = framework.CreateMeshBuffers(cube_mesh.ToMeshData());
        }

        if (!meshBuffers) {
            std::wcerr << L"Failed to create buffers!" << std::endl;
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
            
            // Mouse rotation
            auto mouseOffset = input_device.GetMouseOffset();
            if (mouseOffset.x != 0 || mouseOffset.y != 0) {
                yaw += mouseOffset.x * mouseSensitivity;
                pitch += mouseOffset.y * mouseSensitivity;

                // Clamp pitch
                if (pitch > 1.5f) pitch = 1.5f;
                if (pitch < -1.5f) pitch = -1.5f;
            }

            // Recalculate forward vector from yaw/pitch
            // Initial camera direction is towards -Z (0, 0, -1) in RH, but lookAtLH.
            // Let's assume standard FPS:
            // x = cos(yaw) * cos(pitch)
            // y = sin(pitch)
            // z = sin(yaw) * cos(pitch)
            
            // Adjust for initial camera orientation if needed.
            // Camera starts at (0, 1.5, -5) looking at (0, 0, 0).
            // Forward is (0, -1.5, 5). Normalized: (0, -0.28, 0.96).
            // This is roughly looking forward +Z and slightly down.
            
            // If we start yaw/pitch at 0, 0.
            // forward = (sin(yaw)*cos(pitch), sin(pitch), cos(yaw)*cos(pitch))
            // yaw=0, pitch=0 -> (0, 0, 1) -> +Z.
            
            // If first frame, initialize yaw/pitch from current forward
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
