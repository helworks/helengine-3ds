#include "platform/3ds/Nintendo3DsBootHost.hpp"

namespace helengine::nintendo3ds {
    /// Creates the Nintendo 3DS bootstrap host with no initialized render targets.
    Nintendo3DsBootHost::Nintendo3DsBootHost()
        : TopTarget(nullptr)
        , BottomTarget(nullptr) {
    }

    /// Initializes the Nintendo 3DS renderer stack and presents the verification colors until shutdown.
    int Nintendo3DsBootHost::Run() {
        if (!InitializeRenderer()) {
            ShutdownRenderer();
            return 1;
        }

        while (aptMainLoop()) {
            hidScanInput();
            PresentFrame();
        }

        ShutdownRenderer();
        return 0;
    }

    /// Initializes libctru, citro3d, citro2d, and the screen render targets.
    bool Nintendo3DsBootHost::InitializeRenderer() {
        gfxInitDefault();

        if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
            return false;
        }

        if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
            return false;
        }

        C2D_Prepare();

        TopTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        BottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

        if (TopTarget == nullptr) {
            return false;
        } else if (BottomTarget == nullptr) {
            return false;
        }

        return true;
    }

    /// Draws one verification frame to both visible screens.
    void Nintendo3DsBootHost::PresentFrame() {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(TopTarget, TopScreenColor);
        C2D_SceneBegin(TopTarget);

        C2D_TargetClear(BottomTarget, BottomScreenColor);
        C2D_SceneBegin(BottomTarget);

        C3D_FrameEnd(0);
    }

    /// Shuts down the renderer stack in reverse initialization order.
    void Nintendo3DsBootHost::ShutdownRenderer() {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
    }
}
