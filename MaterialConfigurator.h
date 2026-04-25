#pragma once

#include <memory>
#include "framework/Framework.h"
#include "SceneConfig.h"
#include "MeshData.h"

namespace gfw {

class TextureResolver;

// Структура для хранения загруженных подмеш-объектов
struct LoadedSubmesh {
    MeshBuffers *mesh = nullptr;
    std::wstring texture_path;
    DirectX::XMFLOAT4 albedo = {1.0f, 1.0f, 1.0f, 1.0f};
};

class MaterialConfigurator {
public:
    static void ConfigureMaterial(
        RenderObject &obj,
        MaterialMode mode,
        const SceneObjectConfig &config,
        const LoadedSubmesh &submesh,
        TextureResolver &resolver,
        Framework &framework,
        const RenderSettings &render_settings);

private:
    static void ConfigureTexturedMaterial(
        RenderObject &obj,
        const SceneObjectConfig &config,
        const LoadedSubmesh &submesh,
        TextureResolver &resolver);

    static void ConfigureSolidColorMaterial(
        RenderObject &obj,
        Framework &framework,
        const SceneObjectConfig &config);

    static void ConfigureRainbowMaterial(
        RenderObject &obj,
        Framework &framework,
        const SceneObjectConfig &config);
};

} // namespace gfw




