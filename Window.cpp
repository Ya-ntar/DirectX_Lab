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
		: m_hWnd(nullptr)
		, m_hInstance(nullptr)
		, m_ClassName(L"DirectXWindowClass")
		, m_IsRunning(false)
		, m_pInputDevice(nullptr)
	{
	}

	Window::~Window() noexcept
	{
		Destroy();
	}

	bool Window::Create(const WindowDesc& desc)
	{
		m_Desc = desc;
		m_hInstance = desc.HInstance ? desc.HInstance : GetModuleHandle(nullptr);

		RegisterWindowClass();

		m_hWnd = CreateWindowExW(
			desc.ExStyle,
			m_ClassName.c_str(),
			desc.Title.c_str(),
			desc.Style,
			desc.X,
			desc.Y,
			desc.Width,
			desc.Height,
			nullptr,
			nullptr,
			m_hInstance,
			this
		);

		if (!m_hWnd)
		{
			std::wcerr << L"Failed to create window. Error: " << GetLastError() << std::endl;
			UnregisterWindowClass();
			return false;
		}

		ShowWindow(m_hWnd, SW_SHOW);
		UpdateWindow(m_hWnd);

		m_IsRunning = true;
		return true;
	}

	void Window::Destroy()
	{
		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
			m_hWnd = nullptr;
		}
		UnregisterWindowClass();
		m_IsRunning = false;
	}

	int Window::Run()
	{
		MSG msg = {};
		while (m_IsRunning)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					m_IsRunning = false;
					return static_cast<int>(msg.wParam);
				}

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
				m_IsRunning = false;
				return;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		Window* pWindow = GetWindowFromHWND(hwnd);

		if (uMsg == WM_NCCREATE)
		{
			auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
			pWindow = static_cast<Window*>(pCreate->lpCreateParams);
			if (pWindow)
			{
				SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
			}
			return pWindow ? TRUE : FALSE;
		}

		if (pWindow)
		{
			if (uMsg == WM_INPUT && pWindow->m_pInputDevice)
			{
				UINT dwSize = 0;
				GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
				
				if (dwSize > 0)
				{
					std::vector<BYTE> buffer(dwSize);
					if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
					{
						auto* raw = reinterpret_cast<RAWINPUT*>(buffer.data());

						if (raw->header.dwType == RIM_TYPEKEYBOARD)
						{
							InputDevice::KeyboardInputEventArgs args{};
							args.MakeCode = raw->data.keyboard.MakeCode;
							args.Flags = raw->data.keyboard.Flags;
							args.VKey = raw->data.keyboard.VKey;
							args.Message = raw->data.keyboard.Message;

							pWindow->m_pInputDevice->OnKeyDown(args);
						}
						else if (raw->header.dwType == RIM_TYPEMOUSE)
						{
							InputDevice::RawMouseEventArgs args{};
							args.Mode = raw->data.mouse.usFlags;
							args.ButtonFlags = raw->data.mouse.usButtonFlags;
							args.ExtraInformation = static_cast<int>(raw->data.mouse.ulExtraInformation);
							args.Buttons = static_cast<int>(raw->data.mouse.ulRawButtons);
							args.WheelDelta = static_cast<short>(raw->data.mouse.usButtonData);
							args.X = raw->data.mouse.lLastX;
							args.Y = raw->data.mouse.lLastY;

							pWindow->m_pInputDevice->OnMouseMove(args);
						}
					}
				}
				return DefWindowProc(hwnd, uMsg, wParam, lParam);
			}

			if (uMsg == WM_CLOSE)
			{
				pWindow->m_IsRunning = false;
				DestroyWindow(hwnd);
				return 0;
			}

			if (uMsg == WM_DESTROY)
			{
				pWindow->m_IsRunning = false;
				PostQuitMessage(0);
				return 0;
			}
		}

		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	Window* Window::GetWindowFromHWND(HWND hwnd)
	{
		return reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	void Window::RegisterWindowClass()
	{
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = m_hInstance;
		wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = m_ClassName.c_str();
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
		if (m_hInstance)
		{
			UnregisterClassW(m_ClassName.c_str(), m_hInstance);
		}
	}

	int Window::GetWidth() const
	{
		RECT rect;
		if (GetClientRect(m_hWnd, &rect))
		{
			return rect.right - rect.left;
		}
		return m_Desc.Width;
	}

	int Window::GetHeight() const
	{
		RECT rect;
		if (GetClientRect(m_hWnd, &rect))
		{
			return rect.bottom - rect.top;
		}
		return m_Desc.Height;
	}
}

