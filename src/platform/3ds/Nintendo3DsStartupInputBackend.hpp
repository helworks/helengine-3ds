#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include "IInputBackend.hpp"

namespace helengine::nintendo3ds {
    /// Translates Nintendo 3DS HID state into the shared generated-core input contract during startup-scene materialization.
    class Nintendo3DsStartupInputBackend : public IInputBackend {
    public:
        /// Initializes the Nintendo 3DS startup input backend with persistent reusable gamepad storage.
        Nintendo3DsStartupInputBackend();

        /// Releases the Nintendo 3DS startup input backend and its persistent reusable gamepad storage.
        ~Nintendo3DsStartupInputBackend();

        /// Captures one Nintendo 3DS HID frame for startup-scene materialization.
        /// <returns>Generated-core input frame state populated from the current Nintendo 3DS HID state.</returns>
        InputFrameState CaptureFrame() override;

    private:
        /// Stores the primary reusable single-gamepad array so consecutive frames never alias the same storage.
        Array<InputGamepadState>* PrimaryCachedGamepads;

        /// Stores the alternating reusable single-gamepad array so consecutive frames never alias the same storage.
        Array<InputGamepadState>* SecondaryCachedGamepads;

        /// Stores which reusable gamepad array receives the next captured frame.
        bool UsePrimaryCachedGamepads;

        /// Stores whether the previous captured touch frame was pressed.
        bool PreviousTouchPressed;

        /// Stores the previous touch X coordinate used to calculate pointer deltas.
        int PreviousTouchX;

        /// Stores the previous touch Y coordinate used to calculate pointer deltas.
        int PreviousTouchY;

        /// Stores whether the backend has observed at least one valid touch position yet.
        bool HasPreviousTouchPosition;
    };
}

#endif
