#pragma once

#include "Keys.h"
#include "Delegates.h"
#include "Exports.h"
#include <unordered_set>

#ifndef DIRECTX_SIMPLEMATH_H
namespace DirectX::SimpleMath {
    struct Vector2 {
        float x, y;
        Vector2() : x(0), y(0) {}
        Vector2(float x_, float y_) : x(x_), y(y_) {}
    };
}
#endif

namespace gfw
{
    class Window;

    class GAMEFRAMEWORK_API InputDevice
    {
        friend class gfw::Window;

    public:
        struct MouseMoveEventArgs
        {
            DirectX::SimpleMath::Vector2 position;
            DirectX::SimpleMath::Vector2 offset;
            int wheel_delta{};
        };

        MulticastDelegate<const MouseMoveEventArgs&> mouse_move;

        DirectX::SimpleMath::Vector2 GetMousePosition() const { return mouse_position_; }
        DirectX::SimpleMath::Vector2 GetMouseOffset() const { return mouse_offset_; }
        int GetMouseWheelDelta() const { return mouse_wheel_delta_; }

        explicit InputDevice(HWND hwnd);
        ~InputDevice() noexcept;

        InputDevice(const InputDevice&) = delete;
        InputDevice& operator=(const InputDevice&) = delete;
        InputDevice(InputDevice&&) = delete;
        InputDevice& operator=(InputDevice&&) = delete;

        void AddPressedKey(Keys key);
        void RemovePressedKey(Keys key);
        bool IsKeyDown(Keys key) const;

        struct KeyboardInputEventArgs {
            USHORT make_code;
            USHORT flags;
            USHORT vkey;
            UINT   message;
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
            int mode;
            int button_flags;
            int extra_information;
            int buttons;
            int wheel_delta;
            int x;
            int y;
        };

        void OnKeyDown(const KeyboardInputEventArgs& args);
        void OnMouseMove(const RawMouseEventArgs& args);

    private:
        std::unordered_set<Keys> pressed_keys_;
        HWND handle_;
        DirectX::SimpleMath::Vector2 mouse_position_{};
        DirectX::SimpleMath::Vector2 mouse_offset_{};
        int mouse_wheel_delta_ = 0;
    };
}
