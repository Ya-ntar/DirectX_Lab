#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "InputDevice.h"
#include <iostream>
#include <stdexcept>

using namespace gfw;
using namespace DirectX::SimpleMath;

InputDevice::InputDevice(HWND hwnd)
        : handle_(hwnd) {
    if (handle_ == nullptr) {
        throw std::invalid_argument("InputDevice requires a valid HWND");
    }

    RAWINPUTDEVICE rid[2];

    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x02;
    rid[0].dwFlags = 0;
    rid[0].hwndTarget = handle_;

    rid[1].usUsagePage = 0x01;
    rid[1].usUsage = 0x06;
    rid[1].dwFlags = 0;
    rid[1].hwndTarget = handle_;

    if (RegisterRawInputDevices(rid, 2, sizeof(rid[0])) == FALSE) {
        DWORD error_code = GetLastError();
        std::wcerr << L"ERROR: Failed to register raw input devices. Error code: " << error_code << std::endl;
        throw std::runtime_error("Failed to register raw input devices");
    }
}

InputDevice::~InputDevice() noexcept = default;

void InputDevice::OnKeyDown(const KeyboardInputEventArgs &args) {
    constexpr USHORT LEFT_SHIFT_MAKE_CODE = 42;
    constexpr USHORT RIGHT_SHIFT_MAKE_CODE = 54;
    constexpr USHORT LEFT_CTRL_MAKE_CODE = 29;
    constexpr USHORT RIGHT_CTRL_MAKE_CODE = 285;
    constexpr USHORT LEFT_ALT_MAKE_CODE = 56;
    constexpr USHORT RIGHT_ALT_MAKE_CODE = 312;
    constexpr USHORT KEY_BREAK_FLAG = 0x01;

    bool is_break = (args.flags & KEY_BREAK_FLAG);

    Keys key = static_cast<Keys>(args.vkey);

    if (args.make_code == LEFT_SHIFT_MAKE_CODE) {
        key = Keys::LeftShift;
    } else if (args.make_code == RIGHT_SHIFT_MAKE_CODE) {
        key = Keys::RightShift;
    } else if (args.make_code == LEFT_CTRL_MAKE_CODE) {
        key = Keys::LeftControl;
    } else if (args.make_code == RIGHT_CTRL_MAKE_CODE) {
        key = Keys::RightControl;
    } else if (args.make_code == LEFT_ALT_MAKE_CODE) {
        key = Keys::LeftAlt;
    } else if (args.make_code == RIGHT_ALT_MAKE_CODE) {
        key = Keys::RightAlt;
    }

    if (is_break) {
        pressed_keys_.erase(key);
    } else {
        pressed_keys_.insert(key);
    }
}

void InputDevice::OnMouseMove(const RawMouseEventArgs &args) {
    static const struct { int flag; Keys key; bool down; } kButtons[] = {
        { static_cast<int>(MouseButtonFlags::LeftButtonDown),   Keys::LeftButton,   true  },
        { static_cast<int>(MouseButtonFlags::LeftButtonUp),     Keys::LeftButton,   false },
        { static_cast<int>(MouseButtonFlags::RightButtonDown),   Keys::RightButton,  true  },
        { static_cast<int>(MouseButtonFlags::RightButtonUp),     Keys::RightButton,  false },
        { static_cast<int>(MouseButtonFlags::MiddleButtonDown),  Keys::MiddleButton, true  },
        { static_cast<int>(MouseButtonFlags::MiddleButtonUp),    Keys::MiddleButton, false },
        { static_cast<int>(MouseButtonFlags::Button4Down),       Keys::MouseButtonX1, true  },
        { static_cast<int>(MouseButtonFlags::Button4Up),        Keys::MouseButtonX1, false },
        { static_cast<int>(MouseButtonFlags::Button5Down),       Keys::MouseButtonX2, true  },
        { static_cast<int>(MouseButtonFlags::Button5Up),        Keys::MouseButtonX2, false },
    };
    for (const auto &b : kButtons) {
        if (args.button_flags & b.flag)
            b.down ? AddPressedKey(b.key) : RemovePressedKey(b.key);
    }

    if (args.x != 0 || args.y != 0) {
        mouse_offset_ = Vector2(static_cast<float>(args.x), static_cast<float>(args.y));
    } else {
        mouse_offset_ = Vector2(0.0f, 0.0f);
    }

    if (args.button_flags & static_cast<int>(MouseButtonFlags::MouseWheel)) {
        mouse_wheel_delta_ = args.wheel_delta;
    } else if (args.button_flags & static_cast<int>(MouseButtonFlags::Hwheel)) {
        mouse_wheel_delta_ = args.wheel_delta;
    } else {
        mouse_wheel_delta_ = 0;
    }

    POINT point;
    if (GetCursorPos(&point)) {
        ScreenToClient(handle_, &point);
        mouse_position_ = Vector2(static_cast<float>(point.x), static_cast<float>(point.y));
    }

    if (mouse_move.GetSize() > 0) {
        const MouseMoveEventArgs move_args = {mouse_position_, mouse_offset_, mouse_wheel_delta_};
        mouse_move.Broadcast(move_args);
    }
}

void InputDevice::AddPressedKey(Keys key) {
    pressed_keys_.insert(key);
}

void InputDevice::RemovePressedKey(Keys key) {
    pressed_keys_.erase(key);
}

bool InputDevice::IsKeyDown(Keys key) const {
    return pressed_keys_.find(key) != pressed_keys_.end();
}
