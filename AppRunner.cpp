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

    void AddObjectsToConfig(AppConfig &config) {
        SceneObjectConfig sponza_object;
        sponza_object.name = L"Sponza textured";
        sponza_object.obj_path = L"sponza/Sponza-master/sponza.obj";
        sponza_object.mtl_path = L"sponza/Sponza-master/sponza.mtl";
        sponza_object.material_mode = MaterialMode::Texture;
        sponza_object.position = {0.0f, 0.0f, 0.0f};
        sponza_object.scale = {1.0f, 1.0f, 1.0f};
        config.objects.push_back(sponza_object);
    }

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

        auto it = texture_cache.find(effective_path);
        if (it != texture_cache.end()) {
            return it->second;
        }

        std::shared_ptr<Texture2D> texture = framework.CreateTextureFromFile(effective_path);
        texture_cache[effective_path] = texture;
        return texture ? texture : framework.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
    }
}


std::vector<LoadedSubmesh> LoadModelWithCache(
        const SceneObjectConfig &obj,
        std::unordered_map<std::wstring, std::vector<LoadedSubmesh>> &model_cache,
        std::vector<std::unique_ptr<MeshBuffers>> &mesh_buffers,
        Framework &framework) {
    const std::wstring key = obj.obj_path + L"|" + obj.mtl_path;

    // ---------- Cache hit ----------
    if (auto it = model_cache.find(key); it != model_cache.end()) {
        return it->second;
    }

    // ---------- Load model ----------
    ObjModelData model = MeshLoader::LoadObjModel(obj.obj_path, obj.mtl_path);
    std::vector<LoadedSubmesh> result;

    for (auto &sub: model.submeshes) {
        if (sub.mesh.vertex_count == 0 || sub.mesh.indices.empty())
            continue;

        auto buffers = framework.CreateMeshBuffers(sub.mesh);
        if (!buffers)
            continue;

        result.push_back({.mesh = buffers.get(), .texture_path = sub.diffuse_texture_path, .albedo = sub.albedo});

        mesh_buffers.push_back(std::move(buffers));
    }

    // ---------- Cache store ----------
    model_cache[key] = result;

    return result;
}

std::vector<LoadedSubmesh> GetCubeFallback(
        MeshBuffers *&cube_mesh,
        std::vector<std::unique_ptr<MeshBuffers>> &mesh_buffers,
        Framework &framework) {
    if (!cube_mesh) {
        auto cubeData = CubeMesh::CreateUnit().ToMeshData();

        if (auto buffers = framework.CreateMeshBuffers(cubeData)) {
            cube_mesh = buffers.get();
            mesh_buffers.emplace_back(std::move(buffers));
        }
    }

    if (!cube_mesh)
        return {};

    return {{
                    .mesh = cube_mesh,
                    .texture_path = L"",
                    .albedo = {1, 1, 1, 1}
            }};
}

RenderObject CreateRenderObject(
        const SceneObjectConfig &configObj,
        const LoadedSubmesh &sub,
        Framework &framework,
        std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> &texture_cache) {
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

    return obj;
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

    AddObjectsToConfig(config); // <- scene config

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

        if (!configObj.obj_path.empty()) {
            submeshes = LoadModelWithCache(
                    configObj,
                    model_cache,
                    mesh_buffers,
                    framework
            );
        } else {
            submeshes = GetCubeFallback(cube_mesh, mesh_buffers, framework);
        }

        if (submeshes.empty()) {
            std::wcerr << L"Skipped object '" << configObj.name
                       << L"': failed to create mesh.\n";
            continue;
        }

        // ---------- Create render objects ----------
        for (auto &sub: submeshes) {
            objects.push_back(CreateRenderObject(
                    configObj,
                    sub,
                    framework,
                    texture_cache
            ));
        }
    }

    if (objects.empty()) {
        std::wcerr << L"No valid scene objects configured." <<
                   std::endl;
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
    PrintTessellationAndDebugHelp();

    bool tess_key_pressed = false;
    bool wireframe_key_pressed = false;
    bool debug_0_pressed = false, debug_1_pressed = false, debug_2_pressed = false, debug_3_pressed = false, debug_4_pressed = false;

    while (window.IsRunning()) {
        window.ProcessMessages();
        timer.Tick();
        auto dt = static_cast<float>(timer.GetDeltaTime());
        game.Update(window, input_device, framework, dt);
        ApplyLightControls(input_device, framework.GetSceneState().camera, dt, light_control);
        PushLightsToRenderingSystem(light_control, rendering_system);

        // Tessellation toggle (T key)
        bool t_pressed = input_device.IsKeyDown(Keys::T);
        if (t_pressed && !tess_key_pressed) {
            rendering_system.SetTessellationEnabled(!rendering_system.IsTessellationEnabled());
            std::cout << "Tessellation: " << (rendering_system.IsTessellationEnabled() ? "ENABLED" : "DISABLED") << std::endl;
            tess_key_pressed = true;
        } else if (!t_pressed) {
            tess_key_pressed = false;
        }

        // Wireframe toggle (W key)
        bool w_pressed = input_device.IsKeyDown(Keys::V);
        if (w_pressed && !wireframe_key_pressed) {
            rendering_system.ToggleRenderMode();
            std::cout << "Render Mode: " << (rendering_system.GetRenderMode() == RenderingSystem::RenderMode::Wireframe ? "WIREFRAME" : "SOLID") << std::endl;
            wireframe_key_pressed = true;
        } else if (!w_pressed) {
            wireframe_key_pressed = false;
        }

        // GBuffer debug modes
        bool d0_pressed = input_device.IsKeyDown(Keys::D0);
        if (d0_pressed && !debug_0_pressed) {
            rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::None);
            std::cout << "GBuffer Debug: OFF" << std::endl;
            debug_0_pressed = true;
        } else if (!d0_pressed) {
            debug_0_pressed = false;
        }

        bool d1_pressed = input_device.IsKeyDown(Keys::D1);
        if (d1_pressed && !debug_1_pressed) {
            rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::Position);
            std::cout << "GBuffer Debug: POSITION" << std::endl;
            debug_1_pressed = true;
        } else if (!d1_pressed) {
            debug_1_pressed = false;
        }

        bool d2_pressed = input_device.IsKeyDown(Keys::D2);
        if (d2_pressed && !debug_2_pressed) {
            rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::Normal);
            std::cout << "GBuffer Debug: NORMAL" << std::endl;
            debug_2_pressed = true;
        } else if (!d2_pressed) {
            debug_2_pressed = false;
        }

        bool d3_pressed = input_device.IsKeyDown(Keys::D3);
        if (d3_pressed && !debug_3_pressed) {
            rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::Albedo);
            std::cout << "GBuffer Debug: ALBEDO" << std::endl;
            debug_3_pressed = true;
        } else if (!d3_pressed) {
            debug_3_pressed = false;
        }

        bool d4_pressed = input_device.IsKeyDown(Keys::D4);
        if (d4_pressed && !debug_4_pressed) {
            rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::Depth);
            std::cout << "GBuffer Debug: DEPTH (from Position.Z)" << std::endl;
            debug_4_pressed = true;
        } else if (!d4_pressed) {
            debug_4_pressed = false;
        }

        framework.BeginFrame();
        rendering_system.Render(objects, static_cast<float>(timer.GetTotalTime()));
        framework.EndFrame();
    }

    rendering_system.Shutdown();

    framework.Shutdown();
    return true;
}

