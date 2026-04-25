#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "framework/Framework.h"

namespace gfw {

struct SceneObjectConfig;
class Framework;

class TextureResolver {
public:
    explicit TextureResolver(Framework &framework);

    // Разрешает диффузную текстуру с кешированием
    std::shared_ptr<Texture2D> ResolveDiffuse(
        const SceneObjectConfig &config,
        const std::wstring &fallback_texture_path);

    // Загружает нормаль-маппинг текстуру по явному пути
    std::shared_ptr<Texture2D> ResolveNormal(const std::wstring &texture_stem);

    // Загружает displacement текстуру по явному пути
    std::shared_ptr<Texture2D> ResolveDisplacement(const std::wstring &texture_stem);

    // Очищает кеш текстур
    void ClearCache() { cache_.clear(); }

private:
    Framework &framework_;
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> cache_;

    // Вспомогательный метод для поиска файла с несколькими расширениями
    std::shared_ptr<Texture2D> TryLoadTexture(
        const std::wstring &directory,
        const std::vector<std::wstring> &patterns);

    // Извлекает стем имени файла из полного пути (без директории и расширения)
    static std::wstring ExtractFileStem(const std::wstring &full_path);
};

} // namespace gfw



