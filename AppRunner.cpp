#include "AppRunner.h"

#include <iostream>
#include <cstddef>
#include <memory>
#include <vector>
#include <unordered_map>

#include "ControlSettings.h"
#include "CubeMesh.h"
#include "GameController.h"
#include "MeshData.h"
#include "MeshLoader.h"
#include "RenderingSystem.h"
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

struct LightControlState {
    std::vector<PointLight> point_lights;
    std::vector<SpotLight> spot_lights;
    size_t active_point = 0;
    size_t active_spot = 0;
    size_t enabled_point_count = 0;
    size_t enabled_spot_count = 0;
    float move_speed = 5.0f;
    bool edit_spot = false;
    std::unordered_map<int, bool> key_latch;

    bool PressedOnce(InputDevice &input, Keys key) {
        const int code = static_cast<int>(key);
        const bool down = input.IsKeyDown(key);
        const bool was_down = key_latch[code];
        key_latch[code] = down;
        return down && !was_down;
    }
};

void Normalize3(DirectX::XMFLOAT3 &v) {
    const DirectX::XMVECTOR vec = DirectX::XMVectorSet(v.x, v.y, v.z, 0.0f);
    const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(vec);
    DirectX::XMStoreFloat3(&v, n);
}

void ApplyLightControls(InputDevice &input, const Camera &camera, float dt, LightControlState &state) {
    if (state.PressedOnce(input, Keys::Tab)) {
        state.edit_spot = !state.edit_spot;
        std::wcout << (state.edit_spot ? L"[Light] Editing SPOT lights\n" : L"[Light] Editing POINT lights\n");
    }

    if (state.PressedOnce(input, Keys::OemPlus)) {
        if (state.edit_spot) {
            const size_t next = state.enabled_spot_count + 1;
            state.enabled_spot_count = (next < state.spot_lights.size()) ? next : state.spot_lights.size();
        } else {
            const size_t next = state.enabled_point_count + 1;
            state.enabled_point_count = (next < state.point_lights.size()) ? next : state.point_lights.size();
        }
    }
    if (state.PressedOnce(input, Keys::OemMinus)) {
        if (state.edit_spot) {
            if (state.enabled_spot_count > 0) state.enabled_spot_count--;
        } else {
            if (state.enabled_point_count > 0) state.enabled_point_count--;
        }
    }

    if (state.PressedOnce(input, Keys::PageUp)) {
        if (state.edit_spot && !state.spot_lights.empty()) {
            state.active_spot = (state.active_spot + 1) % state.spot_lights.size();
        } else if (!state.edit_spot && !state.point_lights.empty()) {
            state.active_point = (state.active_point + 1) % state.point_lights.size();
        }
    }
    if (state.PressedOnce(input, Keys::PageDown)) {
        if (state.edit_spot && !state.spot_lights.empty()) {
            state.active_spot = (state.active_spot + state.spot_lights.size() - 1) % state.spot_lights.size();
        } else if (!state.edit_spot && !state.point_lights.empty()) {
            state.active_point = (state.active_point + state.point_lights.size() - 1) % state.point_lights.size();
        }
    }

    float speed_mul = input.IsKeyDown(Keys::LeftShift) || input.IsKeyDown(Keys::RightShift) ? 3.0f : 1.0f;
    const float move_step = state.move_speed * speed_mul * dt;

    const DirectX::XMVECTOR eye = DirectX::XMLoadFloat3(&camera.position);
    const DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&camera.target);
    DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, eye));
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, forward));

    DirectX::XMVECTOR move = DirectX::XMVectorZero();
    if (input.IsKeyDown(Keys::J)) move = DirectX::XMVectorSubtract(move, right);
    if (input.IsKeyDown(Keys::L)) move = DirectX::XMVectorAdd(move, right);
    if (input.IsKeyDown(Keys::I)) move = DirectX::XMVectorAdd(move, forward);
    if (input.IsKeyDown(Keys::K)) move = DirectX::XMVectorSubtract(move, forward);
    if (input.IsKeyDown(Keys::U)) move = DirectX::XMVectorAdd(move, up);
    if (input.IsKeyDown(Keys::O)) move = DirectX::XMVectorSubtract(move, up);

    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(move)) > 1e-5f) {
        move = DirectX::XMVectorScale(DirectX::XMVector3Normalize(move), move_step);
        DirectX::XMFLOAT3 delta = {};
        DirectX::XMStoreFloat3(&delta, move);

        if (state.edit_spot && !state.spot_lights.empty()) {
            SpotLight &active = state.spot_lights[state.active_spot];
            active.position.x += delta.x;
            active.position.y += delta.y;
            active.position.z += delta.z;
        } else if (!state.point_lights.empty()) {
            PointLight &active = state.point_lights[state.active_point];
            active.position.x += delta.x;
            active.position.y += delta.y;
            active.position.z += delta.z;
        }
    }

    if (state.edit_spot && !state.spot_lights.empty()) {
        SpotLight &active = state.spot_lights[state.active_spot];
        if (input.IsKeyDown(Keys::F)) active.direction.x -= dt;
        if (input.IsKeyDown(Keys::H)) active.direction.x += dt;
        if (input.IsKeyDown(Keys::T)) active.direction.z += dt;
        if (input.IsKeyDown(Keys::G)) active.direction.z -= dt;
        if (input.IsKeyDown(Keys::R)) active.direction.y += dt;
        if (input.IsKeyDown(Keys::Y)) active.direction.y -= dt;
        Normalize3(active.direction);
    }
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
            RenderObject object = {};
            object.mesh = submesh.mesh;
            object.world = MakeWorldMatrix(object_config.position, object_config.scale);
            object.uv_params = {2.0f, 2.0f, 0.08f, -0.05f};
            object.DisableUVAnimation();

            switch (object_config.material_mode) {
                case MaterialMode::Texture:
                    object.texture = ResolveTexture(framework, object_config, submesh.texture_path, texture_cache);
                    object.albedo = submesh.albedo;
                    object.effect_params = {0.0f, 0.0f, 0.0f, 0.0f};
                    break;
                case MaterialMode::SolidColor:
                    object.texture = framework.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
                    object.albedo = object_config.solid_color;
                    object.effect_params = {0.0f, 0.0f, 0.0f, 0.0f};
                    break;
                case MaterialMode::Rainbow:
                    object.texture = framework.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
                    object.albedo = {1.0f, 1.0f, 1.0f, 1.0f};
                    object.effect_params = {1.0f, object_config.rainbow_speed, 0.0f, 0.0f};
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

    RenderingSystem rendering_system;
    if (!rendering_system.Initialize(&framework, window.GetWidth(), window.GetHeight())) {
        std::wcerr << L"Failed to initialize deferred RenderingSystem." << std::endl;
        return false;
    }
    rendering_system.SetDirectionalLight(framework.GetSceneState().light);
    LightControlState light_control = {};
    light_control.point_lights = {
        {{-8.0f, 3.0f, -1.0f}, 12.0f, {1.0f, 0.7f, 0.5f}, 2.2f},
        {{-2.0f, 2.2f, 6.0f}, 10.0f, {0.5f, 0.8f, 1.0f}, 1.8f},
        {{6.0f, 2.8f, -5.0f}, 11.0f, {0.7f, 1.0f, 0.6f}, 2.0f},
        {{11.0f, 3.5f, 3.0f}, 13.0f, {1.0f, 0.4f, 0.9f}, 2.1f}
    };
    light_control.spot_lights = {
        {{0.0f, 8.0f, 0.0f}, 25.0f, {0.0f, -1.0f, 0.0f}, 0.88f, {1.0f, 0.95f, 0.8f}, 2.4f},
        {{-10.0f, 5.0f, -10.0f}, 22.0f, {0.6f, -0.6f, 0.6f}, 0.90f, {0.7f, 0.8f, 1.0f}, 2.1f}
    };
    light_control.enabled_point_count = light_control.point_lights.size();
    light_control.enabled_spot_count = light_control.spot_lights.size();
    rendering_system.SetPointLights(light_control.point_lights);
    rendering_system.SetSpotLights(light_control.spot_lights);

    std::wcout << L"Light controls:\n"
               << L"  Tab - switch Point/Spot edit mode\n"
               << L"  +/- - increase/decrease enabled lights of current type\n"
               << L"  PageUp/PageDown - select active light\n"
               << L"  IJKL + U/O - move active light (camera-local)\n"
               << L"  Shift - faster move\n"
               << L"  For Spot: R/Y + T/F/G/H - adjust direction\n";

    while (window.IsRunning()) {
        window.ProcessMessages();
        timer.Tick();
        const float dt = static_cast<float>(timer.GetDeltaTime());

        game.Update(window, input_device, framework, dt);
        ApplyLightControls(input_device, framework.GetSceneState().camera, dt, light_control);
        rendering_system.SetPointLights(std::vector<PointLight>(
            light_control.point_lights.begin(),
            light_control.point_lights.begin() + static_cast<std::ptrdiff_t>(light_control.enabled_point_count)));
        rendering_system.SetSpotLights(std::vector<SpotLight>(
            light_control.spot_lights.begin(),
            light_control.spot_lights.begin() + static_cast<std::ptrdiff_t>(light_control.enabled_spot_count)));

        framework.BeginFrame();
        rendering_system.Render(objects, static_cast<float>(timer.GetTotalTime()));
        framework.EndFrame();
    }

    rendering_system.Shutdown();
    framework.Shutdown();
    return true;
}
