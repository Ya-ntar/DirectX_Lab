#include <windows.h>
#include <iostream>
#include "Window.h"
#include "InputDevice.h"

using namespace gfw;

int main()
{
    Window window;
    Window::WindowDesc desc;
    desc.Title = L"DirectX 12 Window";
    desc.Width = 1280;
    desc.Height = 720;
    desc.HInstance = GetModuleHandle(nullptr);

    if (!window.Create(desc))
    {
        std::cerr << "Failed to create window!" << std::endl;
        return -1;
    }

    try
    {
        InputDevice inputDevice(window.GetHWND());
        window.SetInputDevice(&inputDevice);

        inputDevice.MouseMove.AddLambda([](const InputDevice::MouseMoveEventArgs& args) {
            std::cout << "Mouse Position: (" << args.Position.x << ", " << args.Position.y << ")" << std::endl;
        });

        int exitCode = window.Run();
        return exitCode;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
}
