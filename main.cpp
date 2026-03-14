#include <windows.h>
#include <iostream>
#include <memory>
#include "framework/Window.h"
#include "framework/InputDevice.h"
#include "framework/Framework.h"
#include "framework/Timer.h"
#include "GameController.h"
#include "CubeMesh.h"
#include "MeshData.h"
#include "MeshLoader.h"

using namespace gfw;

static bool SetupAndRun(Window &window, InputDevice &input_device) {
    Framework framework;
    if (!framework.Initialize(&window)) {
        std::wcerr << L"Failed to initialize Framework!" << std::endl;
        return false;
    }

    MeshData meshData = MeshLoader::LoadObj(L"sponza.obj");
    if (meshData.vertex_count == 0) {
        std::wcerr << L"Sponza not found or empty, using cube." << std::endl;
        meshData = CubeMesh::CreateUnit().ToMeshData();
    }
    std::unique_ptr<MeshBuffers> meshBuffers = framework.CreateMeshBuffers(meshData);
    if (!meshBuffers) {
        std::wcerr << L"Failed to create mesh buffers!" << std::endl;
        return false;
    }

    Timer timer;
    timer.Reset();
    GameController game;

    while (window.IsRunning()) {
        window.ProcessMessages();
        timer.Tick();
        const float dt = static_cast<float>(timer.GetDeltaTime());

        game.Update(window, input_device, framework, dt);

        framework.BeginFrame();
        framework.ClearRenderTarget(0.39f, 0.58f, 0.93f, 1.0f);
        framework.RenderMesh(*meshBuffers, DirectX::XMMatrixIdentity(), timer.GetTotalTime());
        framework.EndFrame();
    }

    framework.Shutdown();
    return true;
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
        if (!SetupAndRun(window, input_device))
            return -1;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}
