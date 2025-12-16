#pragma once

#include "Keys.h"
#include "Delegates.h"
#include "Exports.h"
#include <unordered_set>

#ifndef DIRECTX_SIMPLEMATH_H
namespace DirectX { namespace SimpleMath {
	struct Vector2 {
		float x, y;
		Vector2() : x(0), y(0) {}
		Vector2(float x_, float y_) : x(x_), y(y_) {}
	};
}}
#endif

namespace gfw
{
	class Window;

	class GAMEFRAMEWORK_API InputDevice
	{
		friend class gfw::Window;

		std::unordered_set<Keys>* keys;
		HWND m_hWnd;

	public:

		struct MouseMoveEventArgs
		{
			DirectX::SimpleMath::Vector2 Position;
			DirectX::SimpleMath::Vector2 Offset;
			int WheelDelta;
		};

		DirectX::SimpleMath::Vector2 MousePosition{};
		DirectX::SimpleMath::Vector2 MouseOffset{};
		int MouseWheelDelta = 0;

		MulticastDelegate<const MouseMoveEventArgs&> MouseMove;

	public:

		explicit InputDevice(HWND hWnd);
		~InputDevice();


		void AddPressedKey(Keys key);
		void RemovePressedKey(Keys key);
		bool IsKeyDown(Keys key);

	public:
		struct KeyboardInputEventArgs {
			USHORT MakeCode;
			USHORT Flags;
			USHORT VKey;
			UINT   Message;
		};

		enum class MouseButtonFlags
		{
			LeftButtonDown = 1,
			LeftButtonUp = 2,
			RightButtonDown = 4,
			RightButtonUp = 8,
			MiddleButtonDown = 16,
			MiddleButtonUp = 32,
			Button1Down = LeftButtonDown,
			Button1Up = LeftButtonUp,
			Button2Down = RightButtonDown,
			Button2Up = RightButtonUp,
			Button3Down = MiddleButtonDown,
			Button3Up = MiddleButtonUp,
			Button4Down = 64,
			Button4Up = 128,
			Button5Down = 256,
			Button5Up = 512,
			MouseWheel = 1024,
			Hwheel = 2048,
			None = 0,
		};
		struct RawMouseEventArgs
		{
			int Mode;
			int ButtonFlags;
			int ExtraInformation;
			int Buttons;
			int WheelDelta;
			int X;
			int Y;
		};

		void OnKeyDown(KeyboardInputEventArgs args);
		void OnMouseMove(RawMouseEventArgs args);
	};
}
