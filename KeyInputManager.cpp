#include "KeyInputManager.h"

namespace gfw {

void KeyInputManager::RegisterKeyBinding(Keys key, KeyCallback on_pressed) {
    bindings_.push_back({key, on_pressed, false});
}

void KeyInputManager::Update(const InputDevice &input_device) {
    for (auto &binding : bindings_) {
        bool is_pressed = input_device.IsKeyDown(binding.key);

        // Срабатывает только при переходе из отпущена -> нажата
        if (is_pressed && !binding.was_pressed) {
            binding.callback();
        }

        binding.was_pressed = is_pressed;
    }
}

void KeyInputManager::Clear() {
    bindings_.clear();
}

} // namespace gfw

