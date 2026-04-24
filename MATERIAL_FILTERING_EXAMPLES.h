#pragma once

/*
EXAMPLE: How to render ONLY plants from Sponza

Copy this code into AppConfig.cpp in the BuildAppConfig() function
to replace the current setup and load only vegetation from Sponza.

IMPORTANT: First run PrintModelMaterials() once to see what materials
actually exist in your Sponza.obj file, then update the filter list accordingly.
*/

namespace gfw {

// Example 1: Minimal setup - just load Sponza and print materials
AppConfig BuildAppConfigPrintMaterials() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    // This will print all materials to console on startup
    // PrintModelMaterials(L"sponza/Sponza-master/Sponza.obj",
    //                     L"sponza/Sponza-master/Sponza.mtl");

    // Load the full Sponza without filtering
    SceneObjectConfig sponza;
    sponza.name = L"Sponza Full";
    sponza.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza.material_mode = MaterialMode::Texture;
    // material_filter is empty = load ALL materials
    config.objects.push_back(sponza);

    return config;
}

// Example 2: Load ONLY plants from Sponza
// NOTE: You MUST update the material names based on what PrintModelMaterials() outputs!
AppConfig BuildAppConfigPlantsOnly() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    SceneObjectConfig sponza_plants;
    sponza_plants.name = L"Sponza Plants Only";
    sponza_plants.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_plants.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_plants.material_mode = MaterialMode::Texture;

    // IMPORTANT: Replace these with actual material names from your Sponza!
    // These are just examples. Run PrintModelMaterials() first!
    sponza_plants.material_filter = {
        L"plant",       // Common names
        L"Plant",
        L"vegetation",
        L"Vegetation",
        L"ivy",
        L"Ivy",
        L"leaves",
        L"Leaves",
        L"bush",
        L"Bush",
        L"vines",
        L"Vines"
    };

    config.objects.push_back(sponza_plants);

    return config;
}

// Example 3: Split Sponza into multiple parts for separate rendering/manipulation
AppConfig BuildAppConfigMultipleFilter() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    // Load PLANTS
    SceneObjectConfig sponza_plants;
    sponza_plants.name = L"Sponza Plants";
    sponza_plants.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_plants.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_plants.material_mode = MaterialMode::Texture;
    sponza_plants.material_filter = { L"ivy", L"leaves" }; // Update with actual names
    config.objects.push_back(sponza_plants);

    // Load ARCHITECTURE (walls, ceiling, etc.)
    SceneObjectConfig sponza_arch;
    sponza_arch.name = L"Sponza Architecture";
    sponza_arch.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_arch.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_arch.material_mode = MaterialMode::Texture;
    sponza_arch.material_filter = { L"walls", L"ceiling", L"floor" }; // Update with actual names
    config.objects.push_back(sponza_arch);

    return config;
}

}  // namespace gfw

