#include <windows.h>
#include <iostream>
#include "Window.h"
#include "InputDevice.h"
#include "Keys.h"

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
        std::wcerr << L"Failed to create window!" << std::endl;
        return -1;
    }

    try
    {
        InputDevice inputDevice(window.GetHWND());
        window.SetInputDevice(&inputDevice);


        inputDevice.MouseMove.AddLambda([](const InputDevice::MouseMoveEventArgs& args) {
            std::wcout << L"Mouse Position: (" << args.Position.x << L", " << args.Position.y << L")";
            std::wcout << L" | Offset: (" << args.Offset.x << L", " << args.Offset.y << L")";
            if (args.WheelDelta != 0)
            {
                std::wcout << L" | Wheel: " << args.WheelDelta;
            }
            std::wcout << std::endl;
        });

        std::wcout << L"Window created successfully. Size: " << window.GetWidth() 
                   << L"x" << window.GetHeight() << std::endl;
        std::wcout << L"Move mouse and press keys to test input. Press ESC to exit." << std::endl;

        int exitCode = window.Run();
        return exitCode;
    }
    catch (const std::exception& e)
    {
        std::wcerr << L"Error: " << e.what() << std::endl;
        return -1;
    }
}
