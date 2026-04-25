#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "SceneConfig.h"
#include "AssetRepository.h"
#include "framework/FrameworkTypes.h"

namespace gfw {

class Framework;

class MaterialResolver {
public:
    explicit MaterialResolver(Framework &framework);

    void ApplyMaterial(const SceneObjectConfig &object_config,
                       const LoadedSubmesh &submesh,
                       RenderObject &render_object);

private:
    std::shared_ptr<Texture2D> ResolveTexture(const SceneObjectConfig &object_config,
                                              const std::wstring &fallback_texture_path);

    Framework &framework_;
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> texture_cache_;
    std::shared_ptr<Texture2D> white_texture_;
};

}  // namespace gfw

