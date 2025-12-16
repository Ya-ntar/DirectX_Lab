#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "InputDevice.h"
#include <iostream>
#include <stdexcept>

using namespace gfw;
using namespace DirectX::SimpleMath;

InputDevice::InputDevice(HWND hWnd)
	: m_hWnd(hWnd)
{
	if (m_hWnd == nullptr)
	{
		throw std::invalid_argument("InputDevice requires a valid HWND");
	}

	RAWINPUTDEVICE Rid[2];

	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x02;
	Rid[0].dwFlags = 0;
	Rid[0].hwndTarget = m_hWnd;

	Rid[1].usUsagePage = 0x01;
	Rid[1].usUsage = 0x06;
	Rid[1].dwFlags = 0;
	Rid[1].hwndTarget = m_hWnd;

	if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
	{
		DWORD errorCode = GetLastError();
		std::wcerr << L"ERROR: Failed to register raw input devices. Error code: " << errorCode << std::endl;
		throw std::runtime_error("Failed to register raw input devices");
	}
}

InputDevice::~InputDevice() noexcept = default;

void InputDevice::OnKeyDown(const KeyboardInputEventArgs& args)
{
	constexpr USHORT LEFT_SHIFT_MAKE_CODE = 42;
	constexpr USHORT RIGHT_SHIFT_MAKE_CODE = 54;
	constexpr USHORT LEFT_CTRL_MAKE_CODE = 29;
	constexpr USHORT RIGHT_CTRL_MAKE_CODE = 285; // 0x11D
	constexpr USHORT LEFT_ALT_MAKE_CODE = 56;
	constexpr USHORT RIGHT_ALT_MAKE_CODE = 312; // 0x138
	constexpr USHORT KEY_BREAK_FLAG = 0x01;

	bool isBreak = (args.Flags & KEY_BREAK_FLAG);

	Keys key = static_cast<Keys>(args.VKey);

	if (args.MakeCode == LEFT_SHIFT_MAKE_CODE)
	{
		key = Keys::LeftShift;
	}
	else if (args.MakeCode == RIGHT_SHIFT_MAKE_CODE)
	{
		key = Keys::RightShift;
	}
	else if (args.MakeCode == LEFT_CTRL_MAKE_CODE)
	{
		key = Keys::LeftControl;
	}
	else if (args.MakeCode == RIGHT_CTRL_MAKE_CODE)
	{
		key = Keys::RightControl;
	}
	else if (args.MakeCode == LEFT_ALT_MAKE_CODE)
	{
		key = Keys::LeftAlt;
	}
	else if (args.MakeCode == RIGHT_ALT_MAKE_CODE)
	{
		key = Keys::RightAlt;
	}
	
	if (isBreak)
	{
		keys.erase(key);
	}
	else
	{
		keys.insert(key);
	}
}

void InputDevice::OnMouseMove(const RawMouseEventArgs& args)
{
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::LeftButtonDown))
		AddPressedKey(Keys::LeftButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::LeftButtonUp))
		RemovePressedKey(Keys::LeftButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::RightButtonDown))
		AddPressedKey(Keys::RightButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::RightButtonUp))
		RemovePressedKey(Keys::RightButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::MiddleButtonDown))
		AddPressedKey(Keys::MiddleButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::MiddleButtonUp))
		RemovePressedKey(Keys::MiddleButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::Button4Down))
		AddPressedKey(Keys::MouseButtonX1);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::Button4Up))
		RemovePressedKey(Keys::MouseButtonX1);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::Button5Down))
		AddPressedKey(Keys::MouseButtonX2);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::Button5Up))
		RemovePressedKey(Keys::MouseButtonX2);

	if (args.X != 0 || args.Y != 0)
	{
		MouseOffset = Vector2(static_cast<float>(args.X), static_cast<float>(args.Y));
	}
	else
	{
		MouseOffset = Vector2(0.0f, 0.0f);
	}

	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::MouseWheel))
	{
		MouseWheelDelta = args.WheelDelta;
	}
	else if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::Hwheel))
	{
		MouseWheelDelta = args.WheelDelta;
	}
	else
	{
		MouseWheelDelta = 0;
	}

	POINT p;
	if (GetCursorPos(&p))
	{
		ScreenToClient(m_hWnd, &p);
		MousePosition = Vector2(static_cast<float>(p.x), static_cast<float>(p.y));
	}

	if (MouseMove.GetSize() > 0)
	{
		const MouseMoveEventArgs moveArgs = {MousePosition, MouseOffset, MouseWheelDelta};
		MouseMove.Broadcast(moveArgs);
	}
}

void InputDevice::AddPressedKey(Keys key)
{
	keys.insert(key);
}

void InputDevice::RemovePressedKey(Keys key)
{
	keys.erase(key);
}

bool InputDevice::IsKeyDown(Keys key) const
{
	return keys.find(key) != keys.end();
}
