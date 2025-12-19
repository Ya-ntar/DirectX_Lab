#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Window.h"
#include "InputDevice.h"
#include "Delegates.h"
#include <iostream>
#include <vector>

namespace gfw
{
    Window::Window()
        : handle_(nullptr)
        , instance_(nullptr)
        , class_name_(L"DirectXWindowClass")
        , is_running_(false)
        , input_device_(nullptr)
        , client_width_(0)
        , client_height_(0)
    {
    }

	Window::~Window() noexcept
	{
		Destroy();
	}

    bool Window::Create(const WindowDesc& desc)
    {
        desc_ = desc;
        instance_ = desc.instance ? desc.instance : GetModuleHandle(nullptr);

        RegisterWindowClass();

        handle_ = CreateWindowExW(
            desc.ex_style,
            class_name_.c_str(),
            desc.title.c_str(),
            desc.style,
            desc.x,
            desc.y,
            desc.width,
            desc.height,
            nullptr,
            nullptr,
            instance_,
            this
        );

        if (!handle_)
        {
            DWORD error = GetLastError();
            std::wcerr << L"Failed to create window. Error: " << error << std::endl;
            UnregisterWindowClass();
            return false;
        }

        RECT client_rect;
        if (GetClientRect(handle_, &client_rect))
        {
            client_width_ = client_rect.right - client_rect.left;
            client_height_ = client_rect.bottom - client_rect.top;
        }
        else
        {
            client_width_ = desc.width;
            client_height_ = desc.height;
        }

        ShowWindow(handle_, SW_SHOW);
        UpdateWindow(handle_);

        is_running_ = true;
        return true;
    }

    void Window::Destroy()
    {
        if (handle_)
        {
            DestroyWindow(handle_);
            handle_ = nullptr;
        }
        UnregisterWindowClass();
        is_running_ = false;
    }

    int Window::Run()
    {
        MSG msg = {};
        while (is_running_)
        {
            BOOL result = GetMessage(&msg, nullptr, 0, 0);
            
            if (result == 0)
            {
                is_running_ = false;
                return static_cast<int>(msg.wParam);
            }
            else if (result == -1)
            {
                DWORD error = GetLastError();
                std::wcerr << L"GetMessage failed. Error: " << error << std::endl;
                is_running_ = false;
                return -1;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        return 0;
    }

    void Window::ProcessMessages()
    {
        MSG msg = {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                is_running_ = false;
                return;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        Window* window = GetWindowFromHandle(hwnd);

        if (uMsg == WM_NCCREATE)
        {
            auto* create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
            window = static_cast<Window*>(create_struct->lpCreateParams);
            if (window)
            {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            }
            return window ? TRUE : FALSE;
        }

        if (window)
        {
            if (uMsg == WM_INPUT && window->input_device_)
            {
                UINT size = 0;
                UINT result = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
                
                if (result == 0 && size > 0)
                {
                    std::vector<BYTE> buffer(size);
                    result = GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER));
                    
                    if (result != UINT(-1) && result == size)
                    {
                        auto* raw = reinterpret_cast<RAWINPUT*>(buffer.data());

                        if (raw->header.dwType == RIM_TYPEKEYBOARD)
                        {
                            InputDevice::KeyboardInputEventArgs args{};
                            args.make_code = raw->data.keyboard.MakeCode;
                            args.flags = raw->data.keyboard.Flags;
                            args.vkey = raw->data.keyboard.VKey;
                            args.message = raw->data.keyboard.Message;

                            window->input_device_->OnKeyDown(args);
                        }
                        else if (raw->header.dwType == RIM_TYPEMOUSE)
                        {
                            InputDevice::RawMouseEventArgs args{};
                            args.mode = raw->data.mouse.usFlags;
                            args.button_flags = raw->data.mouse.usButtonFlags;
                            args.extra_information = static_cast<int>(raw->data.mouse.ulExtraInformation);
                            args.buttons = static_cast<int>(raw->data.mouse.ulRawButtons);
                            args.wheel_delta = static_cast<short>(raw->data.mouse.usButtonData);
                            args.x = raw->data.mouse.lLastX;
                            args.y = raw->data.mouse.lLastY;

                            window->input_device_->OnMouseMove(args);
                        }
                    }
                }

                return 0;
            }

            if (uMsg == WM_CLOSE)
            {
                window->is_running_ = false;
                DestroyWindow(hwnd);
                return 0;
            }

            if (uMsg == WM_DESTROY)
            {
                window->is_running_ = false;
                PostQuitMessage(0);
                return 0;
            }

            if (uMsg == WM_SIZE)
            {
                RECT client_rect;
                if (GetClientRect(hwnd, &client_rect))
                {
                    window->client_width_ = client_rect.right - client_rect.left;
                    window->client_height_ = client_rect.bottom - client_rect.top;
                }
                return 0;
            }
        }

        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    Window* Window::GetWindowFromHandle(HWND hwnd)
    {
        return reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    void Window::RegisterWindowClass()
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = instance_;
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = class_name_.c_str();
        wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

        if (!RegisterClassExW(&wc))
        {
            DWORD error = GetLastError();
            if (error != ERROR_CLASS_ALREADY_EXISTS)
            {
                std::wcerr << L"Failed to register window class. Error: " << error << std::endl;
            }
        }
    }

    void Window::UnregisterWindowClass()
    {
        if (instance_)
        {
            UnregisterClassW(class_name_.c_str(), instance_);
        }
    }

    int Window::GetWidth() const
    {
        if (handle_)
        {
            RECT rect;
            if (GetClientRect(handle_, &rect))
            {
                return rect.right - rect.left;
            }
        }
        return client_width_ > 0 ? client_width_ : desc_.width;
    }

    int Window::GetHeight() const
    {
        if (handle_)
        {
            RECT rect;
            if (GetClientRect(handle_, &rect))
            {
                return rect.bottom - rect.top;
            }
        }
        return client_height_ > 0 ? client_height_ : desc_.height;
    }
}

