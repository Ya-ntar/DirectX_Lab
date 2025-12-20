#include <windows.h>
#include <iostream>
#include "framework/Window.h"
#include "framework/InputDevice.h"
#include "framework/Keys.h"
#include "framework/Framework.h"
#include "framework/Timer.h"
#include "CubeMesh.h"
#include "MeshData.h"
#include <memory>
#include <cmath>

using namespace gfw;

void RenderCubeAtCenter(Framework &framework, const MeshBuffers &cube_buffers, double total_time) {
    const auto t = static_cast<float>(total_time);
    DirectX::XMMATRIX world = DirectX::XMMatrixRotationY(t) * DirectX::XMMatrixRotationX(t * 0.5f);

    RenderObject obj;
    obj.mesh = &cube_buffers;
    DirectX::XMStoreFloat4x4(&obj.world, world);

    const float a = 0.2f + 0.6f * (0.5f + 0.5f * std::sin(t));
    obj.albedo = DirectX::XMFLOAT4(0.85f, 0.25f, 0.25f, a);

    framework.RenderObject(obj, total_time);
}

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


        CubeMesh cube_mesh = CubeMesh::CreateUnit();
        std::unique_ptr<MeshBuffers> cube_buffers = framework.CreateMeshBuffers(cube_mesh.ToMeshData());

        if (!cube_buffers) {
            std::wcerr << L"Failed to create cube buffers!" << std::endl;
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

            framework.BeginFrame();

            framework.ClearRenderTarget(0.39f, 0.58f, 0.93f, 1.0f);

            RenderCubeAtCenter(framework, *cube_buffers, timer.GetTotalTime());

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
