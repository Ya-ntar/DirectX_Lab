#include "AppRunner.h"

#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>

#include "ControlSettings.h"
#include "CubeMesh.h"
#include "GameController.h"
#include "MeshData.h"
#include "MeshLoader.h"
#include "framework/Framework.h"
#include "framework/InputDevice.h"
#include "framework/Timer.h"
#include "framework/Window.h"

using namespace gfw;

namespace {
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
};

struct AppConfig {
    CameraConfig camera = {};
    ControlSettings controls = {};
    std::vector<SceneObjectConfig> objects = {};
};

struct RuntimeObject {
    RenderObject render = {};
};

struct LoadedSubmesh {
    MeshBuffers *mesh = nullptr;
    std::wstring texture_path;
    DirectX::XMFLOAT4 albedo = {1.0f, 1.0f, 1.0f, 1.0f};
};

DirectX::XMFLOAT4X4 MakeWorldMatrix(const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &scale) {
    const DirectX::XMMATRIX world =
        DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) *
        DirectX::XMMatrixTranslation(position.x, position.y, position.z);
    DirectX::XMFLOAT4X4 out = {};
    DirectX::XMStoreFloat4x4(&out, world);
    return out;
}

std::shared_ptr<Texture2D> ResolveTexture(
    Framework &framework,
    const SceneObjectConfig &config,
    const std::wstring &fallback_texture_path,
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> &texture_cache) {
    std::wstring effective_path;
    if (config.material_mode == MaterialMode::Texture) {
        effective_path = !config.texture_path.empty() ? config.texture_path : fallback_texture_path;
    }
    if (effective_path.empty()) {
        return framework.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
    }

    const auto it = texture_cache.find(effective_path);
    if (it != texture_cache.end()) {
        return it->second;
    }

    std::shared_ptr<Texture2D> texture = framework.CreateTextureFromFile(effective_path);
    texture_cache[effective_path] = texture;
    return texture ? texture : framework.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
}
}

bool RunApplication(Window &window, InputDevice &input_device) {
    Framework framework;
    if (!framework.Initialize(&window)) {
        std::wcerr << L"Failed to initialize Framework!" << std::endl;
        return false;
    }

    AppConfig config;
    config.camera.position = {0.0f, 2.0f, -8.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    SceneObjectConfig sponza_object;
    sponza_object.name = L"Sponza textured";
    sponza_object.obj_path = L"sponza/Sponza-master/sponza.obj";
    sponza_object.mtl_path = L"sponza/Sponza-master/sponza.mtl";
    sponza_object.material_mode = MaterialMode::Texture;
    sponza_object.position = {0.0f, 0.0f, 0.0f};
    sponza_object.scale = {1.0f, 1.0f, 1.0f};
    config.objects.push_back(sponza_object);

    SceneObjectConfig solid_cube;
    solid_cube.name = L"Solid cube";
    solid_cube.material_mode = MaterialMode::Rainbow;
    solid_cube.solid_color = {0.90f, 0.25f, 0.25f, 1.0f};
    solid_cube.position = {-2.0f, 1.0f, 0.0f};
    solid_cube.scale = {1.0f, 1.0f, 1.0f};
    config.objects.push_back(solid_cube);

    SceneObjectConfig rainbow_cube;
    rainbow_cube.name = L"Rainbow cube";
    rainbow_cube.material_mode = MaterialMode::Rainbow;
    rainbow_cube.position = {2.0f, 1.0f, 0.0f};
    rainbow_cube.scale = {4.0f, 4.0f, 4.0f};
    rainbow_cube.rainbow_speed = 2.0f;
    config.objects.push_back(rainbow_cube);

    Camera initial_camera;
    initial_camera.position = config.camera.position;
    initial_camera.target = config.camera.target;
    initial_camera.up = {0.0f, 1.0f, 0.0f};
    framework.SetCamera(initial_camera);

    std::vector<std::unique_ptr<MeshBuffers>> mesh_buffers;
    std::unordered_map<std::wstring, std::vector<LoadedSubmesh>> model_cache;
    MeshBuffers *cube_mesh = nullptr;
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> texture_cache;
    std::vector<RuntimeObject> objects;

    for (const SceneObjectConfig &object_config : config.objects) {
        std::vector<LoadedSubmesh> submeshes_to_spawn;
        if (!object_config.obj_path.empty()) {
            const std::wstring mesh_key = object_config.obj_path + L"|" + object_config.mtl_path;
            auto cached = model_cache.find(mesh_key);
            if (cached != model_cache.end()) {
                submeshes_to_spawn = cached->second;
            } else {
                ObjModelData model = MeshLoader::LoadObjModel(object_config.obj_path, object_config.mtl_path);
                std::vector<LoadedSubmesh> loaded_submeshes;
                for (const auto &sub : model.submeshes) {
                    if (sub.mesh.vertex_count == 0 || sub.mesh.indices.empty()) {
                        continue;
                    }
                    std::unique_ptr<MeshBuffers> buffers = framework.CreateMeshBuffers(sub.mesh);
                    if (!buffers) {
                        continue;
                    }
                    loaded_submeshes.push_back(LoadedSubmesh{
                        .mesh = buffers.get(),
                        .texture_path = sub.diffuse_texture_path,
                        .albedo = sub.albedo
                    });
                    mesh_buffers.push_back(std::move(buffers));
                }
                model_cache[mesh_key] = loaded_submeshes;
                submeshes_to_spawn = loaded_submeshes;
            }
        } else {
            if (!cube_mesh) {
                MeshData cube_data = CubeMesh::CreateUnit().ToMeshData();
                std::unique_ptr<MeshBuffers> cube_buffers = framework.CreateMeshBuffers(cube_data);
                if (cube_buffers) {
                    cube_mesh = cube_buffers.get();
                    mesh_buffers.push_back(std::move(cube_buffers));
                }
            }
            if (cube_mesh) {
                submeshes_to_spawn.push_back(LoadedSubmesh{
                    .mesh = cube_mesh,
                    .texture_path = L"",
                    .albedo = {1.0f, 1.0f, 1.0f, 1.0f}
                });
            }
        }

        if (submeshes_to_spawn.empty()) {
            std::wcerr << L"Skipped object '" << object_config.name << L"': failed to create mesh." << std::endl;
            continue;
        }

        for (const LoadedSubmesh &submesh : submeshes_to_spawn) {
            RuntimeObject object = {};
            object.render.mesh = submesh.mesh;
            object.render.world = MakeWorldMatrix(object_config.position, object_config.scale);
            object.render.uv_params = {2.0f, 2.0f, 0.08f, -0.05f};
            object.render.DisableUVAnimation();

            switch (object_config.material_mode) {
                case MaterialMode::Texture:
                    object.render.texture = ResolveTexture(framework, object_config, submesh.texture_path, texture_cache);
                    object.render.albedo = submesh.albedo;
                    object.render.effect_params = {0.0f, 0.0f, 0.0f, 0.0f};
                    break;
                case MaterialMode::SolidColor:
                    object.render.texture = framework.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
                    object.render.albedo = object_config.solid_color;
                    object.render.effect_params = {0.0f, 0.0f, 0.0f, 0.0f};
                    break;
                case MaterialMode::Rainbow:
                    object.render.texture = framework.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
                    object.render.albedo = {1.0f, 1.0f, 1.0f, 1.0f};
                    object.render.effect_params = {1.0f, object_config.rainbow_speed, 0.0f, 0.0f};
                    break;
            }
            objects.push_back(std::move(object));
        }
    }

    if (objects.empty()) {
        std::wcerr << L"No valid scene objects configured." << std::endl;
        return false;
    }

    Timer timer;
    timer.Reset();
    GameControllerSettings game_settings = ToGameControllerSettings(config.controls);
    GameController game(game_settings);

    while (window.IsRunning()) {
        window.ProcessMessages();
        timer.Tick();
        const float dt = static_cast<float>(timer.GetDeltaTime());

        game.Update(window, input_device, framework, dt);

        framework.BeginFrame();
        framework.ClearRenderTarget(0.39f, 0.58f, 0.93f, 1.0f);
        for (auto &object : objects) {
            framework.RenderObject(object.render, timer.GetTotalTime());
        }
        framework.EndFrame();
    }

    framework.Shutdown();
    return true;
}
