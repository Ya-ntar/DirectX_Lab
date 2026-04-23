#include "AppConfig.h"

namespace gfw {

AppConfig BuildAppConfig() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    SceneObjectConfig animated_cube;
    animated_cube.name = L"Animated cube";
    animated_cube.material_mode = MaterialMode::SolidColor;
    animated_cube.solid_color = {0.15f, 0.80f, 0.95f, 1.0f};
    animated_cube.position = {0.0f, 1.0f, 0.0f};
    animated_cube.scale = {1.5f, 1.5f, 1.5f};
    animated_cube.vertex_wave_amplitude = 0.55f;
    animated_cube.vertex_wave_frequency = 4.0f;
    config.objects.push_back(animated_cube);

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

