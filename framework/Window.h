#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <string>
#include "Exports.h"

namespace gfw {
    class InputDevice;

    class GAMEFRAMEWORK_API Window {
    public:
        struct WindowDesc {
            std::wstring title = L"DirectX Window";
            int width = 1280;
            int height = 720;
            int x = CW_USEDEFAULT;
            int y = CW_USEDEFAULT;
            HINSTANCE instance = nullptr;
            DWORD style = WS_OVERLAPPEDWINDOW;
            DWORD ex_style = 0;
        };

    private:
        HWND handle_ = nullptr;
        HINSTANCE instance_ = nullptr;
        std::wstring class_name_;
        WindowDesc desc_;
        bool is_running_ = false;
        InputDevice *input_device_ = nullptr;
        int client_width_ = 0;
        int client_height_ = 0;

        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        static Window *GetWindowFromHandle(HWND hwnd);

        void RegisterWindowClass();

        void UnregisterWindowClass();

    public:
        Window();

        ~Window() noexcept;

        Window(const Window &) = delete;

        Window &operator=(const Window &) = delete;

        Window(Window &&) = delete;

        Window &operator=(Window &&) = delete;

        bool Create(const WindowDesc &desc);

        void Destroy();

        int Run();

        void ProcessMessages();

        HWND GetHandle() const { return handle_; }

        HINSTANCE GetInstance() const { return instance_; }

        bool IsRunning() const { return is_running_; }

        void SetInputDevice(InputDevice *device) { input_device_ = device; }

        InputDevice *GetInputDevice() const { return input_device_; }

        int GetWidth() const;

        int GetHeight() const;
    };
}
