#include "AppRunner.h"

#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>

#include "ControlSettings.h"
#include "GameController.h"
#include "MeshData.h"
#include "MeshLoader.h"
#include "PlaneMesh.h"
#include "RenderingSystem.h"
#include "SceneConfig.h"
#include "SceneLighting.h"
#include "KeyInputManager.h"
#include "TextureResolver.h"
#include "MaterialConfigurator.h"
#include "framework/Framework.h"
#include "framework/InputDevice.h"
#include "framework/Timer.h"
#include "framework/Window.h"


using namespace gfw;

namespace {

    DirectX::XMFLOAT4X4 MakeWorldMatrix(const DirectX::XMFLOAT3 &position, const DirectX::XMFLOAT3 &scale) {
        const DirectX::XMMATRIX world =
                DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) *
                DirectX::XMMatrixTranslation(position.x, position.y, position.z);
        DirectX::XMFLOAT4X4 out = {};
        DirectX::XMStoreFloat4x4(&out, world);
        return out;
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

std::vector<LoadedSubmesh> GetPlaneFallback(
        MeshBuffers *&plane_mesh,
        std::vector<std::unique_ptr<MeshBuffers>> &mesh_buffers,
        Framework &framework) {
    if (!plane_mesh) {
        auto planeData = PlaneMesh::CreateUnit().ToMeshData();

        if (auto buffers = framework.CreateMeshBuffers(planeData)) {
            plane_mesh = buffers.get();
            mesh_buffers.emplace_back(std::move(buffers));
        }
    }

    if (!plane_mesh)
        return {};

    return {{
                    .mesh = plane_mesh,
                    .texture_path = L"",
                    .albedo = {1, 1, 1, 1}
            }};
}

RenderObject CreateRenderObject(
        const SceneObjectConfig &configObj,
        const LoadedSubmesh &sub,
        Framework &framework,
        TextureResolver &texture_resolver,
        const RenderSettings &render_settings) {
    RenderObject obj{};

    obj.mesh = sub.mesh;
    obj.world = MakeWorldMatrix(configObj.position, configObj.scale);
    obj.DisableUVAnimation();

    MaterialConfigurator::ConfigureMaterial(
        obj,
        configObj.material_mode,
        configObj,
        sub,
        texture_resolver,
        framework,
        render_settings);

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
    MeshBuffers *plane_mesh = nullptr;
    TextureResolver texture_resolver(framework);
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
            submeshes = GetPlaneFallback(plane_mesh, mesh_buffers, framework);
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
                    texture_resolver,
                    config.render_settings
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
    SetupDefaultLocalLights(light_control);
    PushLightsToRenderingSystem(light_control, rendering_system);

    // Apply render settings from config
    rendering_system.SetDisplacementScale(config.render_settings.displacement_scale);
    rendering_system.SetNormalDisplacementScale(config.render_settings.normal_displacement_scale);
    rendering_system.SetTessellationParams(config.render_settings.tessellation_min_level,
                                           config.render_settings.tessellation_max_level);
    rendering_system.SetTessellationEnabled(config.render_settings.tessellation_enabled);

    PrintSceneLightingHelp();
    PrintTessellationAndDebugHelp();

    // Setup key input bindings
    KeyInputManager key_manager;

    key_manager.RegisterKeyBinding(Keys::T, [&rendering_system]() {
        rendering_system.SetTessellationEnabled(!rendering_system.IsTessellationEnabled());
        std::cout << "Tessellation: " << (rendering_system.IsTessellationEnabled() ? "ENABLED" : "DISABLED") << std::endl;
    });

    key_manager.RegisterKeyBinding(Keys::V, [&rendering_system]() {
        rendering_system.ToggleRenderMode();
        std::cout << "Render Mode: " << (rendering_system.GetRenderMode() == RenderingSystem::RenderMode::Wireframe ? "WIREFRAME" : "SOLID") << std::endl;
    });

    key_manager.RegisterKeyBinding(Keys::D0, [&rendering_system]() {
        rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::None);
        std::cout << "GBuffer Debug: OFF" << std::endl;
    });

    key_manager.RegisterKeyBinding(Keys::D1, [&rendering_system]() {
        rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::Position);
        std::cout << "GBuffer Debug: POSITION" << std::endl;
    });

    key_manager.RegisterKeyBinding(Keys::D2, [&rendering_system]() {
        rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::Normal);
        std::cout << "GBuffer Debug: NORMAL" << std::endl;
    });

    key_manager.RegisterKeyBinding(Keys::D3, [&rendering_system]() {
        rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::Albedo);
        std::cout << "GBuffer Debug: ALBEDO" << std::endl;
    });

    key_manager.RegisterKeyBinding(Keys::D4, [&rendering_system]() {
        rendering_system.SetGBufferDebugMode(RenderingSystem::GBufferDebugMode::Depth);
        std::cout << "GBuffer Debug: DEPTH (from Position.Z)" << std::endl;
    });

    // Main render loop
    while (window.IsRunning()) {
        window.ProcessMessages();
        timer.Tick();
        auto dt = static_cast<float>(timer.GetDeltaTime());

        // Update game controller and lights
        game.Update(window, input_device, framework, dt);
        ApplyLightControls(input_device, framework.GetSceneState().camera, dt, light_control);
        PushLightsToRenderingSystem(light_control, rendering_system);

        // Update key input state
        key_manager.Update(input_device);

        // Render frame
        framework.BeginFrame();
        rendering_system.Render(objects, static_cast<float>(timer.GetTotalTime()));
        framework.EndFrame();
    }

    rendering_system.Shutdown();

    framework.Shutdown();
    return true;
}



