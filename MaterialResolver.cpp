#include "MaterialResolver.h"

#include "framework/Framework.h"

namespace gfw {

MaterialResolver::MaterialResolver(Framework &framework) : framework_(framework) {
    white_texture_ = framework_.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
}

void MaterialResolver::ApplyMaterial(const SceneObjectConfig &object_config,
                                     const LoadedSubmesh &submesh,
                                     RenderObject &render_object) {
    switch (object_config.material_mode) {
        case MaterialMode::Texture:
            render_object.texture = ResolveTexture(object_config, submesh.texture_path);
            render_object.albedo = submesh.albedo;
            render_object.effect_params = {0.0f, 0.0f, 0.0f, 0.0f};
            break;
        case MaterialMode::SolidColor:
            render_object.texture = white_texture_;
            render_object.albedo = object_config.solid_color;
            render_object.effect_params = {0.0f, 0.0f, 0.0f, 0.0f};
            break;
        case MaterialMode::Rainbow:
            render_object.texture = white_texture_;
            render_object.albedo = {1.0f, 1.0f, 1.0f, 1.0f};
            render_object.effect_params = {1.0f, object_config.rainbow_speed, 0.0f, 0.0f};
            break;
    }
}

std::shared_ptr<Texture2D> MaterialResolver::ResolveTexture(const SceneObjectConfig &object_config,
                                                            const std::wstring &fallback_texture_path) {
    std::wstring effective_path;
    if (object_config.material_mode == MaterialMode::Texture) {
        effective_path = object_config.texture_path.empty() ? fallback_texture_path : object_config.texture_path;
    }

    if (effective_path.empty()) {
        return white_texture_;
    }

    const auto cache_it = texture_cache_.find(effective_path);
    if (cache_it != texture_cache_.end()) {
        return cache_it->second;
    }

    std::shared_ptr<Texture2D> texture = framework_.CreateTextureFromFile(effective_path);
    if (!texture) {
        texture = white_texture_;
    }
    texture_cache_[effective_path] = texture;
    return texture;
}

}  // namespace gfw

