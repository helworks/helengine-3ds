#include "platform/3ds/Nintendo3DsStartupInputBackend.hpp"

#include <3ds.h>

#include <algorithm>

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

namespace helengine::nintendo3ds {
    namespace {
        /// Stores the Nintendo 3DS circle pad range used to normalize analog values into the generated-core signed-stick range.
        constexpr double Nintendo3DsCirclePadRange = 155.0;

        /// Stores the New Nintendo 3DS C-stick range used to normalize analog values into the generated-core signed-stick range.
        constexpr double Nintendo3DsCStickRange = 145.0;

        /// Stores the generated-core full-scale signed analog magnitude.
        constexpr double Nintendo3DsAnalogFullScale = 32767.0;
    }

    /// Initializes the Nintendo 3DS startup input backend with persistent reusable gamepad storage.
    Nintendo3DsStartupInputBackend::Nintendo3DsStartupInputBackend()
        : PrimaryCachedGamepads(new Array<InputGamepadState>(1))
        , SecondaryCachedGamepads(new Array<InputGamepadState>(1))
        , UsePrimaryCachedGamepads(true)
        , PreviousTouchPressed(false)
        , PreviousTouchX(0)
        , PreviousTouchY(0)
        , HasPreviousTouchPosition(false) {
    }

    /// Releases the Nintendo 3DS startup input backend and its persistent reusable gamepad storage.
    Nintendo3DsStartupInputBackend::~Nintendo3DsStartupInputBackend() {
        delete PrimaryCachedGamepads;
        PrimaryCachedGamepads = nullptr;
        delete SecondaryCachedGamepads;
        SecondaryCachedGamepads = nullptr;
    }

    /// Captures one Nintendo 3DS HID frame for startup-scene materialization.
    /// <returns>Generated-core input frame state populated from the current Nintendo 3DS HID state.</returns>
    InputFrameState Nintendo3DsStartupInputBackend::CaptureFrame() {
        u32 heldKeys = hidKeysHeld();
        touchPosition touch {};
        circlePosition circlePad {};
#ifdef KEY_CSTICK_LEFT
        circlePosition cStick {};
#endif
        hidTouchRead(&touch);
        hidCircleRead(&circlePad);
#ifdef KEY_CSTICK_LEFT
        irrstCstickRead(&cStick);
#endif

        const bool touchIsDown = (heldKeys & KEY_TOUCH) != 0;

        int touchX = HasPreviousTouchPosition ? PreviousTouchX : 0;
        int touchY = HasPreviousTouchPosition ? PreviousTouchY : 0;
        int touchDeltaX = 0;
        int touchDeltaY = 0;
        if (touchIsDown) {
            touchX = touch.px;
            touchY = touch.py;
            if (HasPreviousTouchPosition && PreviousTouchPressed) {
                touchDeltaX = touchX - PreviousTouchX;
                touchDeltaY = touchY - PreviousTouchY;
            }

            PreviousTouchX = touchX;
            PreviousTouchY = touchY;
            HasPreviousTouchPosition = true;
        }

        InputFrameState frame {};
        frame.Keyboard = KeyboardState();
        frame.Text = InputTextState();

        InputPointerState pointerState {};
        pointerState.Connected = true;
        pointerState.X = touchX;
        pointerState.Y = touchY;
        pointerState.DeltaX = touchIsDown ? touchDeltaX : 0;
        pointerState.DeltaY = touchIsDown ? touchDeltaY : 0;
        pointerState.ScrollDelta = 0;
        pointerState.SetButtonDown(InputPointerButton::Primary, touchIsDown);
        frame.Pointer = pointerState;

        frame.Mouse = MouseState(
            touchX,
            touchY,
            0,
            touchIsDown ? ButtonState::Pressed : ButtonState::Released,
            ButtonState::Released,
            ButtonState::Released,
            ButtonState::Released,
            ButtonState::Released);

        Array<InputGamepadState>* gamepadStorage = UsePrimaryCachedGamepads ? PrimaryCachedGamepads : SecondaryCachedGamepads;
        frame.Gamepads = gamepadStorage;
        frame.GamepadCount = 1;

        InputGamepadState gamepadState {};
        gamepadState.Connected = true;
        gamepadState.SetButtonDown(InputGamepadButton::DPadUp, (heldKeys & KEY_DUP) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadDown, (heldKeys & KEY_DDOWN) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadLeft, (heldKeys & KEY_DLEFT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::DPadRight, (heldKeys & KEY_DRIGHT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::South, (heldKeys & KEY_A) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::East, (heldKeys & KEY_B) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::West, (heldKeys & KEY_X) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::North, (heldKeys & KEY_Y) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::LeftShoulder, (heldKeys & KEY_L) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::RightShoulder, (heldKeys & KEY_R) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::Start, (heldKeys & KEY_START) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::Select, (heldKeys & KEY_SELECT) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::LeftTrigger, (heldKeys & KEY_ZL) != 0);
        gamepadState.SetButtonDown(InputGamepadButton::RightTrigger, (heldKeys & KEY_ZR) != 0);
        gamepadState.LeftStickX = static_cast<int16_t>(std::clamp(static_cast<double>(circlePad.dx) / Nintendo3DsCirclePadRange, -1.0, 1.0) * Nintendo3DsAnalogFullScale);
        gamepadState.LeftStickY = static_cast<int16_t>(std::clamp(static_cast<double>(circlePad.dy) / Nintendo3DsCirclePadRange, -1.0, 1.0) * Nintendo3DsAnalogFullScale);
        gamepadState.LeftTrigger = (heldKeys & KEY_ZL) != 0 ? 32767 : 0;
        gamepadState.RightTrigger = (heldKeys & KEY_ZR) != 0 ? 32767 : 0;
        gamepadState.RightStickX = 0;
        gamepadState.RightStickY = 0;

#ifdef KEY_CSTICK_LEFT
        gamepadState.RightStickX = static_cast<int16_t>(std::clamp(static_cast<double>(cStick.dx) / Nintendo3DsCStickRange, -1.0, 1.0) * Nintendo3DsAnalogFullScale);
        gamepadState.RightStickY = static_cast<int16_t>(std::clamp(static_cast<double>(cStick.dy) / Nintendo3DsCStickRange, -1.0, 1.0) * Nintendo3DsAnalogFullScale);
#endif

        gamepadStorage->Data[0] = gamepadState;
        UsePrimaryCachedGamepads = !UsePrimaryCachedGamepads;
        PreviousTouchPressed = touchIsDown;
        return frame;
    }
}

#endif
