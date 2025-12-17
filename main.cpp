#include <windows.h>
#include <iostream>
#include "Window.h"
#include "InputDevice.h"
#include "Keys.h"
#include "Framework.h"
#include "Timer.h"

using namespace gfw;

int main() {
    Window window;
    Window::WindowDesc desc;
    desc.title = L"DirectX 12 Window";
    desc.width = 1280;
    desc.height = 720;
    desc.instance = GetModuleHandle(nullptr);

    if (!window.Create(desc)) {
        std::wcerr << L"Failed to create window!" << std::endl;
        return -1;
    }

    try {
        InputDevice input_device(window.GetHandle());
        window.SetInputDevice(&input_device);

        Framework framework;
        if (!framework.Initialize(&window)) {
            std::wcerr << L"Failed to initialize Framework!" << std::endl;
            return -1;
        }

        Timer timer;
        timer.Reset();

        std::wcout << L"Window created successfully. Size: " << window.GetWidth()
                   << L"x" << window.GetHeight() << std::endl;
        std::wcout << L"DirectX 12 initialized. Press ESC to exit." << std::endl;

        while (window.IsRunning()) {
            window.ProcessMessages();

            timer.Tick();

            framework.BeginFrame();

            framework.ClearRenderTarget(0.39f, 0.58f, 0.93f, 1.0f);

            framework.EndFrame();
        }

        framework.Shutdown();
        return 0;
    }
    catch (const std::exception &e) {
        std::wcerr << L"Error: " << e.what() << std::endl;
        return -1;
    }
}
