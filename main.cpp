#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <iostream>
#include "Window.h"
#include "InputDevice.h"

using namespace gfw;

int main()
{
    ID3D12Device* device = nullptr;

    HRESULT hr = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device)
    );

    if (SUCCEEDED(hr))
    {
        std::cout << "DX12 device created!" << std::endl;
        device->Release();
    }
    else
    {
        std::cout << "Failed: " << std::hex << hr << std::endl;
    }

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

    InputDevice inputDevice(window.GetHWND());
    window.SetInputDevice(&inputDevice);

    inputDevice.MouseMove.AddLambda([](const InputDevice::MouseMoveEventArgs& args) {
        std::cout << "Mouse Position: (" << args.Position.x << ", " << args.Position.y << ")" << std::endl;
    });

    int exitCode = window.Run();

    return exitCode;
}
