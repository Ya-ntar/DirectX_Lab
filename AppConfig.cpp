#include "AppConfig.h"
#include "MeshLoader.h"
#include <iostream>

namespace gfw {

// Debug function to print all materials in a model
void PrintModelMaterials(const std::wstring &obj_path, const std::wstring &mtl_path) {
    std::vector<std::wstring> materials = MeshLoader::GetMaterialNames(obj_path, mtl_path);
    std::wcout << L"Materials in " << obj_path << L":" << std::endl;
    for (const auto &mat : materials) {
        std::wcout << L"  - " << mat << std::endl;
    }
}

AppConfig BuildAppConfig() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};



    PrintModelMaterials(L"sponza/Sponza-master/Sponza.obj", L"sponza/Sponza-master/Sponza.mtl");
    SceneObjectConfig sponza_plants;

    SceneObjectConfig sponza_else;
    sponza_else.name = L"Sponza (filtered)";
    sponza_else.material_mode = MaterialMode::Texture;
    sponza_else.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_else.mtl_path = L"sponza/Sponza-master/Sponza.mtl";

    sponza_else.material_filter = {
            L"column_b",
            L"fabric_c",
            L"fabric_g",
            L"flagpole",
            L"column_c",
            L"fabric_f",
            L"floor",
            L"bricks",
            L"column_a",
            L"details",
            L"vase_hanging",
            L"chain",
            L"arch",
            L"ceiling",
            L"fabric_e",
            L"fabric_d",
            L"vase_round",
            L"Material__57",
            L"Material__298",
            L"Material__47",
            L"fabric_a",
            L"Material__25",
            L"vase",
            L"roof"
    };
    config.objects.push_back(sponza_else);

    sponza_plants.name = L"Sponza Plants";
    sponza_plants.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_plants.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_plants.material_mode = MaterialMode::Texture;
    sponza_plants.material_filter = { L"leaf" };

    sponza_plants.vertex_wave_amplitude = 0.14f;
    sponza_plants.vertex_wave_frequency = 2.8f;
    config.objects.push_back(sponza_plants);


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
