/**
 * ПОЛНЫЙ ПРИМЕР: Выделение и отрисовка растений из Sponza
 *
 * Это полный рабочий пример для AppConfig.cpp
 * Скопируйте функцию BuildAppConfig() ниже в AppConfig.cpp для загрузки только растений
 */

#include "AppConfig.h"

namespace gfw {

// ВАРИАНТ 1: Загрузить Sponza и вывести все материалы в консоль
// Используйте этот вариант, чтобы узнать какие материалы соответствуют растениям
AppConfig BuildAppConfigDebug() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    // Загружаем всю Sponza без фильтра
    SceneObjectConfig sponza;
    sponza.name = L"Sponza Full (Debug)";
    sponza.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza.material_mode = MaterialMode::Texture;
    // material_filter остаётся пустым = загружаются ВСЕ материалы
    config.objects.push_back(sponza);

    return config;
}

// ВАРИАНТ 2: Загрузить ТОЛЬКО растения (после того как узнали имена из Варианта 1)
AppConfig BuildAppConfigPlantsOnly() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    // Загружаем ТОЛЬКО растения
    SceneObjectConfig sponza_plants;
    sponza_plants.name = L"Sponza Plants";
    sponza_plants.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_plants.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_plants.material_mode = MaterialMode::Texture;

    // ВАЖНО: Замените эти имена на точные имена из console output в Варианте 1
    // Проверьте точное совпадение (case-sensitive!)
    sponza_plants.material_filter = {
        L"ivy",
        L"leaves",
        L"bush",
        L"plant_leaves",
        L"Vegetation",
    };

    config.objects.push_back(sponza_plants);

    return config;
}

// ВАРИАНТ 3: Разделить Sponza на две части для отдельной визуализации
AppConfig BuildAppConfigSeparateParts() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    // Часть 1: Растения
    SceneObjectConfig sponza_plants;
    sponza_plants.name = L"Sponza Plants";
    sponza_plants.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_plants.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_plants.material_mode = MaterialMode::Texture;
    sponza_plants.material_filter = { L"ivy", L"leaves" };
    // Можно установить разные координаты, масштаб, и т.д.
    // sponza_plants.position = {0.0f, 0.0f, 0.0f};
    // sponza_plants.scale = {1.0f, 1.0f, 1.0f};
    config.objects.push_back(sponza_plants);

    // Часть 2: Архитектура (стены, потолок, пол)
    SceneObjectConfig sponza_arch;
    sponza_arch.name = L"Sponza Architecture";
    sponza_arch.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_arch.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_arch.material_mode = MaterialMode::Texture;
    sponza_arch.material_filter = {
        L"walls",
        L"ceiling",
        L"floor",
        L"fabric",
        L"metal"
    };
    config.objects.push_back(sponza_arch);

    return config;
}

// ВАРИАНТ 4: Загрузить разные части с разными режимами отрисовки
AppConfig BuildAppConfigMultiRender() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    // Растения с текстурами
    SceneObjectConfig plants_textured;
    plants_textured.name = L"Plants (Textured)";
    plants_textured.obj_path = L"sponza/Sponza-master/Sponza.obj";
    plants_textured.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    plants_textured.material_mode = MaterialMode::Texture;
    plants_textured.material_filter = { L"ivy", L"leaves" };
    config.objects.push_back(plants_textured);

    // Растения подсвечены сплошным цветом (для отладки)
    SceneObjectConfig plants_solid;
    plants_solid.name = L"Plants (Highlight)";
    plants_solid.obj_path = L"sponza/Sponza-master/Sponza.obj";
    plants_solid.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    plants_solid.material_mode = MaterialMode::SolidColor;
    plants_solid.solid_color = {0.0f, 1.0f, 0.0f, 1.0f}; // Зелёный
    plants_solid.material_filter = { L"ivy", L"leaves" };
    // Небольшой сдвиг, чтобы растения были слегка смещены вперёд
    plants_solid.position = {0.1f, 0.1f, -0.1f};
    config.objects.push_back(plants_solid);

    return config;
}

// === ТЕКУЩАЯ ИСПОЛЬЗУЕМАЯ ФУНКЦИЯ ===
// Замените содержимое BuildAppConfig() на один из вариантов выше
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

    // РАСКОММЕНТИРУЙТЕ ОДИН ИЗ ВАРИАНТОВ НИЖЕ:

    // Вариант 1: Загрузить Sponza и выяснить какие материалы нужны
    // auto sponza = BuildAppConfigDebug();
    // config.objects.insert(config.objects.end(),
    //                        sponza.objects.begin(), sponza.objects.end());

    // Вариант 2: Загрузить только растения
    // auto sponza = BuildAppConfigPlantsOnly();
    // config.objects.insert(config.objects.end(),
    //                        sponza.objects.begin(), sponza.objects.end());

    // Вариант 3: Разделить на растения и архитектуру
    // auto sponza = BuildAppConfigSeparateParts();
    // config.objects.insert(config.objects.end(),
    //                        sponza.objects.begin(), sponza.objects.end());

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

