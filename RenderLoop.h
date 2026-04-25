#pragma once

#include <vector>

#include "framework/FrameworkTypes.h"
#include "framework/Timer.h"

namespace gfw {

class Window;
class InputDevice;
class Framework;
class GameController;

void RenderFrame(Framework &framework, std::vector<RenderObject> &objects, double total_time);

void RunRenderLoop(Window &window,
                   InputDevice &input_device,
                   Framework &framework,
                   GameController &game,
                   std::vector<RenderObject> &objects,
                   Timer &timer);

}  // namespace gfw

