#pragma once

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

namespace helengine::nintendo3ds {
    /// Owns the first Nintendo 3DS renderer bootstrap and verification frame loop.
    class Nintendo3DsBootHost {
    public:
        /// Creates the Nintendo 3DS bootstrap host with no initialized render targets.
        Nintendo3DsBootHost();

        /// Initializes the Nintendo 3DS renderer stack and presents the verification colors until shutdown.
        int Run();

    private:
        /// Stores the top-screen clear color.
        static constexpr u32 TopScreenColor = C2D_Color32(0x00, 0xFF, 0x00, 0xFF);

        /// Stores the bottom-screen clear color.
        static constexpr u32 BottomScreenColor = C2D_Color32(0x00, 0xFF, 0xFF, 0xFF);

        /// Stores the top-screen render target.
        C3D_RenderTarget* TopTarget;

        /// Stores the bottom-screen render target.
        C3D_RenderTarget* BottomTarget;

        /// Initializes libctru, citro3d, citro2d, and the screen render targets.
        bool InitializeRenderer();

        /// Draws one verification frame to both visible screens.
        void PresentFrame();

        /// Shuts down the renderer stack in reverse initialization order.
        void ShutdownRenderer();
    };
}
