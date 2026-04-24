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

    SceneObjectConfig sponza_plants;
    sponza_plants.name = L"Sponza Plants";
    sponza_plants.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_plants.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_plants.material_mode = MaterialMode::Texture;
    sponza_plants.material_filter = { L"leaf" };  // ТОЛЬКО РАСТЕНИЯ!
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

