#pragma once

#include <functional>
#include <unordered_map>
#include "framework/InputDevice.h"

namespace gfw {

class KeyInputManager {
public:
    using KeyCallback = std::function<void()>;

    // Регистрирует обработчик для клавиши с отслеживанием повторного нажатия
    void RegisterKeyBinding(Keys key, KeyCallback on_pressed);

    // Обновляет состояние всех зарегистрированных клавиш
    void Update(const InputDevice &input_device);

    // Очищает все привязки
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

