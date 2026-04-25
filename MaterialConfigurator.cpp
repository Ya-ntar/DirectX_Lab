#include "MaterialConfigurator.h"
#include "TextureResolver.h"
#include "SceneConfig.h"
#include "MeshData.h"

namespace gfw {

void MaterialConfigurator::ConfigureMaterial(
    RenderObject &obj,
    MaterialMode mode,
    const SceneObjectConfig &config,
    const LoadedSubmesh &submesh,
    TextureResolver &resolver,
    Framework &framework,
    const RenderSettings &render_settings) {

    obj.uv_params = {render_settings.uv_scale.x, render_settings.uv_scale.y,
                     render_settings.uv_offset.x, render_settings.uv_offset.y};

    switch (mode) {
        case MaterialMode::Texture:
            ConfigureTexturedMaterial(obj, config, submesh, resolver);
            break;

        case MaterialMode::SolidColor:
            ConfigureSolidColorMaterial(obj, framework, config);
            break;

        case MaterialMode::Rainbow:
            ConfigureRainbowMaterial(obj, framework, config);
            break;
    }
}

void MaterialConfigurator::ConfigureTexturedMaterial(
    RenderObject &obj,
    const SceneObjectConfig &config,
    const LoadedSubmesh &submesh,
    TextureResolver &resolver) {

    // Разрешаем диффузную текстуру
    obj.texture = resolver.ResolveDiffuse(config, submesh.texture_path);
    obj.albedo = submesh.albedo;

    // Определяем источник для поиска нормалей и displacement
    const std::wstring &texture_source =
        !config.texture_path.empty() ? config.texture_path : submesh.texture_path;

    if (!texture_source.empty()) {
        // Пытаемся загрузить нормаль-маппинг
        if (auto normal_tex = resolver.ResolveNormal(texture_source)) {
            obj.normal_texture = normal_tex;
        }

        // Пытаемся загрузить displacement
        if (auto disp_tex = resolver.ResolveDisplacement(texture_source)) {
            obj.displacement_texture = disp_tex;
        }
    }
}

void MaterialConfigurator::ConfigureSolidColorMaterial(
    RenderObject &obj,
    Framework &framework,
    const SceneObjectConfig &config) {
    obj.texture = framework.CreateSolidTexture({1, 1, 1, 1});
    obj.albedo = config.solid_color;
}

void MaterialConfigurator::ConfigureRainbowMaterial(
    RenderObject &obj,
    Framework &framework,
    const SceneObjectConfig &config) {
    obj.texture = framework.CreateSolidTexture({1, 1, 1, 1});
    obj.albedo = {1, 1, 1, 1};
    obj.effect_params = {1.0f, config.rainbow_speed, 0.0f, 0.0f};
}

} // namespace gfw



