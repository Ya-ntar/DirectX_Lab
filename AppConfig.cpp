#include "AppConfig.h"

namespace gfw {

AppConfig BuildAppConfig() {
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

    return config;
}

Camera BuildInitialCamera(const AppConfig &config) {
    Camera camera;
    camera.position = config.camera.position;
    camera.target = config.camera.target;
    camera.up = {0.0f, 1.0f, 0.0f};
    return camera;
}

}  // namespace gfw

