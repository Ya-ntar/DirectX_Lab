#include "TextureResolver.h"
#include "SceneConfig.h"

namespace gfw {

TextureResolver::TextureResolver(Framework &framework) : framework_(framework) {}

std::shared_ptr<Texture2D> TextureResolver::ResolveDiffuse(
    const SceneObjectConfig &config,
    const std::wstring &fallback_texture_path) {

    std::wstring effective_path;
    if (config.material_mode == MaterialMode::Texture) {
        effective_path = !config.texture_path.empty() ? config.texture_path : fallback_texture_path;
    }

    if (effective_path.empty()) {
        return framework_.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
    }

    // Проверяем кеш
    auto it = cache_.find(effective_path);
    if (it != cache_.end()) {
        return it->second;
    }

    // Загружаем и кешируем
    std::shared_ptr<Texture2D> texture = framework_.CreateTextureFromFile(effective_path);
    cache_[effective_path] = texture;
    return texture ? texture : framework_.CreateSolidTexture({1.0f, 1.0f, 1.0f, 1.0f});
}

std::wstring TextureResolver::ExtractFileStem(const std::wstring &full_path) {
    size_t last_slash = full_path.find_last_of(L"/\\");
    std::wstring filename = (last_slash != std::wstring::npos) ? full_path.substr(last_slash + 1) : full_path;

    size_t dot_pos = filename.find_last_of(L'.');
    if (dot_pos != std::wstring::npos) {
        filename = filename.substr(0, dot_pos);
    }

    return filename;
}

std::shared_ptr<Texture2D> TextureResolver::TryLoadTexture(
    const std::wstring &directory,
    const std::vector<std::wstring> &patterns) {

    for (const auto &pattern : patterns) {
        std::wstring test_path = directory + pattern;

        // Проверяем кеш
        if (auto it = cache_.find(test_path); it != cache_.end()) {
            return it->second;
        }

        // Пытаемся загрузить
        std::shared_ptr<Texture2D> texture = framework_.CreateTextureFromFile(test_path);
        if (texture) {
            cache_[test_path] = texture;
            return texture;
        }
    }

    return nullptr;
}

std::shared_ptr<Texture2D> TextureResolver::ResolveNormal(const std::wstring &texture_stem) {
    if (texture_stem.empty()) {
        return nullptr;
    }

    size_t last_slash = texture_stem.find_last_of(L"/\\");
    std::wstring directory = (last_slash != std::wstring::npos) ? texture_stem.substr(0, last_slash + 1) : L"";
    std::wstring filename = ExtractFileStem(texture_stem);

    std::vector<std::wstring> patterns = {
        filename + L"_normal.jpg",
        filename + L"_Normal.jpg",
        filename + L"_normal.png",
        filename + L"_Normal.png",
        filename + L"_n.jpg",
        filename + L"_n.png"
    };

    return TryLoadTexture(directory, patterns);
}

std::shared_ptr<Texture2D> TextureResolver::ResolveDisplacement(const std::wstring &texture_stem) {
    if (texture_stem.empty()) {
        return nullptr;
    }

    size_t last_slash = texture_stem.find_last_of(L"/\\");
    std::wstring directory = (last_slash != std::wstring::npos) ? texture_stem.substr(0, last_slash + 1) : L"";
    std::wstring filename = ExtractFileStem(texture_stem);

    std::vector<std::wstring> patterns = {
        filename + L"_displacement.jpg",
        filename + L"_Displacement.jpg",
        filename + L"_disp.jpg",
        filename + L"_height.jpg",
        filename + L"_Height.jpg"
    };

    return TryLoadTexture(directory, patterns);
}

} // namespace gfw

