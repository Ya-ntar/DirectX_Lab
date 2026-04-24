/*
ГОТОВЫЙ КОНФИГ ДЛЯ ТЕСТИРОВАНИЯ

Скопируйте эту функцию в AppConfig.cpp и замените BuildAppConfig()
чтобы сразу начать видеть результаты.

ШАГИ:
1. Отрежьте всю функцию BuildAppConfig() из AppConfig.cpp
2. Вставьте функцию ниже
3. Скомпилируйте
4. Запустите и посмотрите консоль на вывод материалов
5. Обновите material_filter на основе того, что видите в консоли
*/

#include "AppConfig.h"

namespace gfw {

AppConfig BuildAppConfig() {
    AppConfig config;
    config.camera.position = {0.0f, 1.5f, -6.0f};
    config.camera.target = {0.0f, 1.0f, 0.0f};

    // ========================================================
    // ВАРИАНТ 1: ЗАГРУЗИТЬ ВСЮ SPONZA (для отладки)
    // ========================================================
    // Раскомментируйте, если хотите сначала увидеть всю модель

    // SceneObjectConfig sponza_full;
    // sponza_full.name = L"Sponza Full";
    // sponza_full.obj_path = L"sponza/Sponza-master/Sponza.obj";
    // sponza_full.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    // sponza_full.material_mode = MaterialMode::Texture;
    // // material_filter остаётся пустым = ВСЕ материалы загружаются
    // config.objects.push_back(sponza_full);


    // ========================================================
    // ВАРИАНТ 2: ЗАГРУЗИТЬ ТОЛЬКО РАСТЕНИЯ
    // ========================================================
    // Раскомментируйте, если уже знаете имена материалов растений

    SceneObjectConfig sponza_plants;
    sponza_plants.name = L"Sponza Plants";
    sponza_plants.obj_path = L"sponza/Sponza-master/Sponza.obj";
    sponza_plants.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    sponza_plants.material_mode = MaterialMode::Texture;

    // ВАЖНО: Замените эти имена на точные имена из консоли!
    // Скопируйте из вывода PrintModelMaterials()
    sponza_plants.material_filter = {
        L"ivy",         // ← Замените на реальное имя
        L"leaves",      // ← Замените на реальное имя
        L"bush",        // ← Замените на реальное имя
        L"vegetation",  // ← Замените на реальное имя
    };

    config.objects.push_back(sponza_plants);


    // ========================================================
    // ВАРИАНТ 3: ЗАГРУЗИТЬ РАСТЕНИЯ С ПОДСВЕТКОЙ
    // ========================================================
    // Раскомментируйте, если хотите видеть растения одним цветом

    // SceneObjectConfig sponza_plants_highlight;
    // sponza_plants_highlight.name = L"Sponza Plants (Highlight)";
    // sponza_plants_highlight.obj_path = L"sponza/Sponza-master/Sponza.obj";
    // sponza_plants_highlight.mtl_path = L"sponza/Sponza-master/Sponza.mtl";
    // sponza_plants_highlight.material_mode = MaterialMode::SolidColor;
    // sponza_plants_highlight.solid_color = {0.0f, 1.0f, 0.0f, 1.0f}; // Зелёный
    // sponza_plants_highlight.material_filter = {
    //     L"ivy", L"leaves", L"bush", L"vegetation"
    // };
    // config.objects.push_back(sponza_plants_highlight);


    // ========================================================
    // ОПЦИОНАЛЬНО: АНИМИРОВАННЫЙ КУБ (оставить или удалить)
    // ========================================================

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

