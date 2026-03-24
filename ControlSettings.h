#pragma once

#include "GameController.h"

namespace gfw {

struct ControlSettings {
    float camera_move_speed = 3.5f;
    float camera_mouse_sensitivity = 0.005f;
};

inline GameControllerSettings ToGameControllerSettings(const ControlSettings &settings) {
    GameControllerSettings game_settings;
    game_settings.camera_move_speed = settings.camera_move_speed;
    game_settings.camera_mouse_sensitivity = settings.camera_mouse_sensitivity;
    return game_settings;
}

}
