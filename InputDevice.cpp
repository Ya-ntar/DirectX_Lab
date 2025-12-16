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
		auto errorCode = GetLastError();
		std::cerr << "ERROR: Failed to register raw input devices. Error code: " << errorCode << std::endl;
	}
}

InputDevice::~InputDevice() noexcept = default;

void InputDevice::OnKeyDown(const KeyboardInputEventArgs& args)
{
	constexpr USHORT LEFT_SHIFT_MAKE_CODE = 42;
	constexpr USHORT RIGHT_SHIFT_MAKE_CODE = 54;
	constexpr USHORT KEY_BREAK_FLAG = 0x01;

	bool isBreak = args.Flags & KEY_BREAK_FLAG;

	auto key = static_cast<Keys>(args.VKey);

	if (args.MakeCode == LEFT_SHIFT_MAKE_CODE) key = Keys::LeftShift;
	if (args.MakeCode == RIGHT_SHIFT_MAKE_CODE) key = Keys::RightShift;
	
	if(isBreak) {
		keys.erase(key);
	} else {
		keys.insert(key);
	}
}

void InputDevice::OnMouseMove(const RawMouseEventArgs& args)
{
	if(args.ButtonFlags & static_cast<int>(MouseButtonFlags::LeftButtonDown))
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

	MouseOffset = Vector2(static_cast<float>(args.X), static_cast<float>(args.Y));
	MouseWheelDelta = args.WheelDelta;

	if (MouseMove.GetSize() > 0)
	{
		POINT p;
		GetCursorPos(&p);
		ScreenToClient(m_hWnd, &p);
		MousePosition = Vector2(static_cast<float>(p.x), static_cast<float>(p.y));

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
