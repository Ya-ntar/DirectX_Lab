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

        while (window.IsRunning()) {
            window.ProcessMessages();

            timer.Tick();

            const float dt = static_cast<float>(timer.GetDeltaTime());
            const float speed = 2.0f;

            auto camera = framework.GetSceneState().camera;
            DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&camera.position);
            DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&camera.target);
            DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&camera.up);

            DirectX::XMVECTOR forward = DirectX::XMVectorSubtract(target, pos);
            forward = DirectX::XMVector3Normalize(forward);

            DirectX::XMVECTOR right = DirectX::XMVector3Cross(up, forward);
            right = DirectX::XMVector3Normalize(right);

            DirectX::XMVECTOR movement = DirectX::XMVectorZero();

            if (input_device.IsKeyDown(Keys::W)) {
                movement = DirectX::XMVectorAdd(movement, forward);
            }
            if (input_device.IsKeyDown(Keys::S)) {
                movement = DirectX::XMVectorSubtract(movement, forward);
            }
            if (input_device.IsKeyDown(Keys::A)) {
                movement = DirectX::XMVectorSubtract(movement, right);
            }
            if (input_device.IsKeyDown(Keys::D)) {
                movement = DirectX::XMVectorAdd(movement, right);
            }

            if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(movement)) > 0.00001f) {
                movement = DirectX::XMVector3Normalize(movement);
                movement = DirectX::XMVectorScale(movement, speed * dt);

                pos = DirectX::XMVectorAdd(pos, movement);
                target = DirectX::XMVectorAdd(target, movement);

                DirectX::XMStoreFloat3(&camera.position, pos);
                DirectX::XMStoreFloat3(&camera.target, target);

                framework.SetCamera(camera);
            }

            framework.BeginFrame();

            framework.ClearRenderTarget(0.39f, 0.58f, 0.93f, 1.0f);

            // Render Sponza (or Cube fallback)
            // Use Identity world matrix for Sponza
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
