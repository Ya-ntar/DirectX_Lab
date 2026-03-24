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
#include <unordered_map>

using namespace gfw;

static bool SetupAndRun(Window &window, InputDevice &input_device) {
    Framework framework;
    if (!framework.Initialize(&window)) {
        std::wcerr << L"Failed to initialize Framework!" << std::endl;
        return false;
    }

    ObjModelData model = MeshLoader::LoadObjModel(L"sponza/Sponza-master/sponza.obj", L"sponza/Sponza-master/sponza.mtl");
    std::vector<std::unique_ptr<MeshBuffers>> mesh_buffers;
    std::vector<RenderObject> objects;
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> texture_cache;

    for (const auto &sub : model.submeshes) {
        if (sub.mesh.vertex_count == 0 || sub.mesh.indices.empty()) {
            continue;
        }
        std::unique_ptr<MeshBuffers> buffers = framework.CreateMeshBuffers(sub.mesh);
        if (!buffers) {
            continue;
        }
        std::shared_ptr<Texture2D> texture;
        if (!sub.diffuse_texture_path.empty()) {
            auto it = texture_cache.find(sub.diffuse_texture_path);
            if (it == texture_cache.end()) {
                texture = framework.CreateTextureFromFile(sub.diffuse_texture_path);
                texture_cache[sub.diffuse_texture_path] = texture;
            } else {
                texture = it->second;
            }
        }
        if (!texture) {
            texture = framework.CreateSolidTexture(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        }

        RenderObject object;
        object.mesh = buffers.get();
        object.texture = texture;
        object.albedo = sub.albedo;
        object.uv_params = {2.0f, 2.0f, 0.08f, -0.05f};
        objects.push_back(object);
        mesh_buffers.push_back(std::move(buffers));
    }

    if (objects.empty()) {
        MeshData cube_data = CubeMesh::CreateUnit().ToMeshData();
        std::unique_ptr<MeshBuffers> cube_buffers = framework.CreateMeshBuffers(cube_data);
        if (!cube_buffers) {
            std::wcerr << L"Failed to create fallback cube buffers!" << std::endl;
            return false;
        }
        RenderObject object;
        object.mesh = cube_buffers.get();
        object.texture = framework.CreateSolidTexture(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        object.uv_params = {2.0f, 2.0f, 0.08f, -0.05f};
        objects.push_back(object);
        mesh_buffers.push_back(std::move(cube_buffers));
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
        for (const auto &object : objects) {
            framework.RenderObject(object, timer.GetTotalTime());
        }
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
