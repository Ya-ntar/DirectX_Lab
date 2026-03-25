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
#include "RenderingSystem.h"
#include "SceneLighting.h"
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

    Camera initial_camera;
    initial_camera.position = config.camera.position;
    initial_camera.target = config.camera.target;
    initial_camera.up = {0.0f, 1.0f, 0.0f};
    framework.SetCamera(initial_camera);

    std::vector<std::unique_ptr<MeshBuffers>> mesh_buffers;
    std::unordered_map<std::wstring, std::vector<LoadedSubmesh>> model_cache;
    MeshBuffers *cube_mesh = nullptr;
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> texture_cache;
    std::vector<RenderObject> objects;

    for (const SceneObjectConfig &configObj: config.objects) {

        std::vector<LoadedSubmesh> submeshes;

        // ---------- Helper: Load model (with caching) ----------
        auto loadModel = [&](const SceneObjectConfig &obj) {
            const std::wstring key = obj.obj_path + L"|" + obj.mtl_path;

            if (auto it = model_cache.find(key); it != model_cache.end()) {
                return it->second;
            }

            ObjModelData model = MeshLoader::LoadObjModel(obj.obj_path, obj.mtl_path);
            std::vector<LoadedSubmesh> result;

            for (const auto &sub: model.submeshes) {
                if (sub.mesh.vertex_count == 0 || sub.mesh.indices.empty())
                    continue;

                auto buffers = framework.CreateMeshBuffers(sub.mesh);
                if (!buffers)
                    continue;

                result.push_back({
                                         .mesh = buffers.get(),
                                         .texture_path = sub.diffuse_texture_path,
                                         .albedo = sub.albedo
                                 });

                mesh_buffers.push_back(std::move(buffers));
            }

            model_cache[key] = result;
            return result;
        };

        // ---------- Helper: Get cube fallback ----------
        auto getCube = [&]() -> std::vector<LoadedSubmesh> {
            if (!cube_mesh) {
                auto cubeData = CubeMesh::CreateUnit().ToMeshData();
                auto buffers = framework.CreateMeshBuffers(cubeData);

                if (buffers) {
                    cube_mesh = buffers.get();
                    mesh_buffers.push_back(std::move(buffers));
                }
            }

            if (!cube_mesh) return {};

            return {
                    {
                            .mesh = cube_mesh,
                            .texture_path = L"",
                            .albedo = {1, 1, 1, 1}
                    }
            };
        };

        // ---------- Load submeshes ----------
        if (!configObj.obj_path.empty()) {
            submeshes = loadModel(configObj);
        } else {
            submeshes = getCube();
        }

        if (submeshes.empty()) {
            std::wcerr << L"Skipped object '" << configObj.name
                       << L"': failed to create mesh.\n";
            continue;
        }

        // ---------- Create render objects ----------
        for (const auto &sub: submeshes) {
            RenderObject obj{};

            obj.mesh = sub.mesh;
            obj.world = MakeWorldMatrix(configObj.position, configObj.scale);
            obj.uv_params = {2.0f, 2.0f, 0.08f, -0.05f};
            obj.DisableUVAnimation();

            switch (configObj.material_mode) {
                case MaterialMode::Texture:
                    obj.texture = ResolveTexture(framework, configObj, sub.texture_path, texture_cache);
                    obj.albedo = sub.albedo;
                    break;

                case MaterialMode::SolidColor:
                    obj.texture = framework.CreateSolidTexture({1, 1, 1, 1});
                    obj.albedo = configObj.solid_color;
                    break;

                case MaterialMode::Rainbow:
                    obj.texture = framework.CreateSolidTexture({1, 1, 1, 1});
                    obj.albedo = {1, 1, 1, 1};
                    obj.effect_params = {1.0f, configObj.rainbow_speed, 0.0f, 0.0f};
                    break;
            }

            objects.push_back(std::move(obj));
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

    RenderingSystem rendering_system;
    if (!rendering_system.Initialize(&framework, window.GetWidth(), window.GetHeight())) {
        std::wcerr << L"Failed to initialize deferred RenderingSystem." << std::endl;
        return false;
    }
    LightControlState light_control = {};
    light_control.directional = framework.GetSceneState().light;
    SetupDefaultLocalLights(light_control);
    PushLightsToRenderingSystem(light_control, rendering_system);

    PrintSceneLightingHelp();

    while (window.IsRunning()) {
        window.ProcessMessages();
        timer.Tick();
        const float dt = static_cast<float>(timer.GetDeltaTime());

        game.Update(window, input_device, framework, dt);
        ApplyLightControls(input_device, framework.GetSceneState().camera, dt, light_control);
        PushLightsToRenderingSystem(light_control, rendering_system);

        framework.BeginFrame();
        rendering_system.Render(objects, static_cast<float>(timer.GetTotalTime()));
        framework.EndFrame();
    }

    rendering_system.Shutdown();
    framework.Shutdown();
    return true;
}
