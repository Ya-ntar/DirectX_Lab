#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include "Exports.h"

namespace gfw
{
	class InputDevice;

	class GAMEFRAMEWORK_API Window
	{
	public:
		struct WindowDesc
		{
			std::wstring Title = L"DirectX Window";
			int Width = 1280;
			int Height = 720;
			int X = CW_USEDEFAULT;
			int Y = CW_USEDEFAULT;
			HINSTANCE HInstance = nullptr;
			DWORD Style = WS_OVERLAPPEDWINDOW;
			DWORD ExStyle = 0;
		};

	private:
		HWND m_hWnd = nullptr;
		HINSTANCE m_hInstance = nullptr;
		std::wstring m_ClassName;
		WindowDesc m_Desc;
		bool m_IsRunning = false;
		InputDevice* m_pInputDevice = nullptr;
		int m_ClientWidth = 0;
		int m_ClientHeight = 0;

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		static Window* GetWindowFromHWND(HWND hwnd);

		void RegisterWindowClass();
		void UnregisterWindowClass();

	public:
		Window();
		~Window() noexcept;

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;
		Window(Window&&) = delete;
		Window& operator=(Window&&) = delete;

		bool Create(const WindowDesc& desc);
		void Destroy();

		int Run();
		void ProcessMessages();

		HWND GetHWND() const { return m_hWnd; }
		HINSTANCE GetHInstance() const { return m_hInstance; }
		bool IsRunning() const { return m_IsRunning; }
		
		void SetInputDevice(InputDevice* pInputDevice) { m_pInputDevice = pInputDevice; }
		InputDevice* GetInputDevice() const { return m_pInputDevice; }

		int GetWidth() const;
		int GetHeight() const;
	};
}
