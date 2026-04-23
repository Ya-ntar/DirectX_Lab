#include "RenderLoop.h"

#include "GameController.h"
#include "framework/Framework.h"
#include "framework/InputDevice.h"
#include "framework/Window.h"

namespace gfw {

void RenderFrame(Framework &framework, std::vector<RenderObject> &objects, double total_time) {
    framework.BeginFrame();
    framework.ClearRenderTarget(0.39f, 0.58f, 0.93f, 1.0f);
    for (auto &object : objects) {
        framework.RenderObject(object, total_time);
    }
    framework.EndFrame();
}

void RunRenderLoop(Window &window,
                   InputDevice &input_device,
                   Framework &framework,
                   GameController &game,
                   std::vector<RenderObject> &objects,
                   Timer &timer) {
    while (window.IsRunning()) {
        window.ProcessMessages();
        timer.Tick();
        const float dt = static_cast<float>(timer.GetDeltaTime());

        game.Update(window, input_device, framework, dt);
        RenderFrame(framework, objects, timer.GetTotalTime());
    }
}

}  // namespace gfw

