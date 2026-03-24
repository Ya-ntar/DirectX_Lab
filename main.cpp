#include <windows.h>
#include <iostream>

#include "AppRunner.h"
#include "framework/Window.h"
#include "framework/InputDevice.h"

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
        if (!RunApplication(window, input_device))
            return -1;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}
