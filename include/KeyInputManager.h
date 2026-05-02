#pragma once

#include <functional>
#include <unordered_map>
#include "../framework/InputDevice.h"

namespace gfw {

class KeyInputManager {
public:
    using KeyCallback = std::function<void()>;

    // Р РµРіРёСЃС‚СЂРёСЂСѓРµС‚ РѕР±СЂР°Р±РѕС‚С‡РёРє РґР»СЏ РєР»Р°РІРёС€Рё СЃ РѕС‚СЃР»РµР¶РёРІР°РЅРёРµРј РїРѕРІС‚РѕСЂРЅРѕРіРѕ РЅР°Р¶Р°С‚РёСЏ
    void RegisterKeyBinding(Keys key, KeyCallback on_pressed);

    // РћР±РЅРѕРІР»СЏРµС‚ СЃРѕСЃС‚РѕСЏРЅРёРµ РІСЃРµС… Р·Р°СЂРµРіРёСЃС‚СЂРёСЂРѕРІР°РЅРЅС‹С… РєР»Р°РІРёС€
    void Update(const InputDevice &input_device);

    // РћС‡РёС‰Р°РµС‚ РІСЃРµ РїСЂРёРІСЏР·РєРё
    void Clear();

private:
    struct KeyBinding {
        Keys key;
        KeyCallback callback;
        bool was_pressed = false;
    };

    std::vector<KeyBinding> bindings_;
};

} // namespace gfw


