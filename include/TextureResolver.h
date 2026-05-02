#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../framework/Framework.h"

namespace gfw {

struct SceneObjectConfig;
class Framework;

class TextureResolver {
public:
    explicit TextureResolver(Framework &framework);

    // Р Р°Р·СЂРµС€Р°РµС‚ РґРёС„С„СѓР·РЅСѓСЋ С‚РµРєСЃС‚СѓСЂСѓ СЃ РєРµС€РёСЂРѕРІР°РЅРёРµРј
    std::shared_ptr<Texture2D> ResolveDiffuse(
        const SceneObjectConfig &config,
        const std::wstring &fallback_texture_path);

    // Р—Р°РіСЂСѓР¶Р°РµС‚ РЅРѕСЂРјР°Р»СЊ-РјР°РїРїРёРЅРі С‚РµРєСЃС‚СѓСЂСѓ РїРѕ СЏРІРЅРѕРјСѓ РїСѓС‚Рё
    std::shared_ptr<Texture2D> ResolveNormal(const std::wstring &texture_stem);

    // Р—Р°РіСЂСѓР¶Р°РµС‚ displacement С‚РµРєСЃС‚СѓСЂСѓ РїРѕ СЏРІРЅРѕРјСѓ РїСѓС‚Рё
    std::shared_ptr<Texture2D> ResolveDisplacement(const std::wstring &texture_stem);

    // РћС‡РёС‰Р°РµС‚ РєРµС€ С‚РµРєСЃС‚СѓСЂ
    void ClearCache() { cache_.clear(); }

private:
    Framework &framework_;
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> cache_;

    // Р’СЃРїРѕРјРѕРіР°С‚РµР»СЊРЅС‹Р№ РјРµС‚РѕРґ РґР»СЏ РїРѕРёСЃРєР° С„Р°Р№Р»Р° СЃ РЅРµСЃРєРѕР»СЊРєРёРјРё СЂР°СЃС€РёСЂРµРЅРёСЏРјРё
    std::shared_ptr<Texture2D> TryLoadTexture(
        const std::wstring &directory,
        const std::vector<std::wstring> &patterns);

    // РР·РІР»РµРєР°РµС‚ СЃС‚РµРј РёРјРµРЅРё С„Р°Р№Р»Р° РёР· РїРѕР»РЅРѕРіРѕ РїСѓС‚Рё (Р±РµР· РґРёСЂРµРєС‚РѕСЂРёРё Рё СЂР°СЃС€РёСЂРµРЅРёСЏ)
    static std::wstring ExtractFileStem(const std::wstring &full_path);
};

} // namespace gfw




