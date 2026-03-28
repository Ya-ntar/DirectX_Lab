#include "SceneConfig.h"

namespace gfw {

void AddObjectsToConfig(AppConfig &config) {
    // Sponza
  /*  SceneObjectConfig sponza_object;
    sponza_object.name = L"Sponza textured";
    sponza_object.obj_path = L"sponza/Sponza-master/sponza.obj";
    sponza_object.mtl_path = L"sponza/Sponza-master/sponza.mtl";
    sponza_object.material_mode = MaterialMode::Texture;
    sponza_object.position = {0.0f, 0.0f, 0.0f};
    sponza_object.scale = {1.0f, 1.0f, 1.0f};
    config.objects.push_back(sponza_object);*/

     // Brick plane
    SceneObjectConfig brick_cube;
    brick_cube.name = L"Brick Plane with Normal Map";
    brick_cube.obj_path = L"bricks2/wall.obj";
    brick_cube.material_mode = MaterialMode::Texture;
    brick_cube.mtl_path = L"bricks2/wall.mtl";
    brick_cube.texture_path = L"bricks2/bricks2.jpg";
    brick_cube.position = {0.0f, 0.0f, 0.0f};
    brick_cube.scale = {40.0f, 40.0f, 40.0f};
    config.objects.push_back(brick_cube);
}

} // namespace gfw


