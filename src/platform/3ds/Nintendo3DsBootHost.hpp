#pragma once

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <string>

#include "platform/3ds/Nintendo3DsStartupManifestReader.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
class Core;
class CoreInitializationOptions;
class ICamera;
class PlatformInfo;
class RenderTarget;
class StandardPlatformInputConfiguration;
#endif

namespace helengine::nintendo3ds {
#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
    class Nintendo3DsStartupInputBackend;
    class Nintendo3DsStartupRenderManager2D;
    class Nintendo3DsStartupRenderManager3D;
#endif

    /// Owns the first Nintendo 3DS renderer bootstrap and startup-scene frame loop.
    class Nintendo3DsBootHost {
    public:
        /// Creates the Nintendo 3DS bootstrap host with no initialized render targets.
        Nintendo3DsBootHost();

        /// Initializes the Nintendo 3DS renderer stack and presents the startup scene until shutdown.
        int Run();

    private:
        /// Stores the top-screen clear color.
        static constexpr u32 TopScreenColor = C2D_Color32(0x00, 0xFF, 0x00, 0xFF);

        /// Stores the bottom-screen clear color.
        static constexpr u32 BottomScreenColor = C2D_Color32(0x00, 0xFF, 0xFF, 0xFF);

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        /// Stores the top-screen checkpoint color shown after generated-core initialization succeeds.
        static constexpr u32 GeneratedCoreTopScreenColor = C2D_Color32(0xFF, 0xFF, 0x00, 0xFF);

        /// Stores the bottom-screen checkpoint color shown after generated-core initialization succeeds.
        static constexpr u32 GeneratedCoreBottomScreenColor = C2D_Color32(0xFF, 0x00, 0xFF, 0xFF);

        /// Stores the top-screen checkpoint color shown after startup-scene materialization succeeds.
        static constexpr u32 StartupSceneTopScreenColor = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);

        /// Stores the bottom-screen checkpoint color shown after startup-scene materialization succeeds.
        static constexpr u32 StartupSceneBottomScreenColor = C2D_Color32(0xFF, 0x80, 0x00, 0xFF);

        /// Stores the top-screen diagnostic color shown when generated-core initialization throws.
        static constexpr u32 GeneratedCoreFailureTopScreenColor = C2D_Color32(0xFF, 0x00, 0x00, 0xFF);

        /// Stores the bottom-screen diagnostic color shown when generated-core initialization throws.
        static constexpr u32 GeneratedCoreFailureBottomScreenColor = C2D_Color32(0x00, 0x00, 0x00, 0xFF);

        /// Stores the top-screen diagnostic color shown when startup-scene loading throws.
        static constexpr u32 StartupSceneFailureTopScreenColor = C2D_Color32(0x00, 0x00, 0xFF, 0xFF);

        /// Stores the bottom-screen diagnostic color shown when startup-scene loading throws.
        static constexpr u32 StartupSceneFailureBottomScreenColor = C2D_Color32(0x00, 0x00, 0x00, 0xFF);

        /// Stores the top-screen diagnostic color shown when the generated-core update step throws.
        static constexpr u32 UpdateFailureTopScreenColor = C2D_Color32(0xFF, 0x80, 0x00, 0xFF);

        /// Stores the bottom-screen diagnostic color shown when the generated-core update step throws.
        static constexpr u32 UpdateFailureBottomScreenColor = C2D_Color32(0x00, 0x00, 0x00, 0xFF);

        /// Stores the top-screen diagnostic color shown when the generated-core draw step throws.
        static constexpr u32 DrawFailureTopScreenColor = C2D_Color32(0xFF, 0x00, 0xFF, 0xFF);

        /// Stores the bottom-screen diagnostic color shown when the generated-core draw step throws.
        static constexpr u32 DrawFailureBottomScreenColor = C2D_Color32(0x00, 0x00, 0x00, 0xFF);

#endif

        /// Stores the top-screen render target.
        C3D_RenderTarget* TopTarget;

        /// Stores the bottom-screen render target.
        C3D_RenderTarget* BottomTarget;

        /// Stores the active top-screen clear color.
        u32 ActiveTopScreenColor;

        /// Stores the active bottom-screen clear color.
        u32 ActiveBottomScreenColor;

        /// Stores the startup-manifest reader used to load packaged RomFS colors.
        Nintendo3DsStartupManifestReader StartupManifestReader;

        /// Stores whether RomFS mounted successfully during initialization.
        bool RomFsInitialized;

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        /// Stores the generated runtime core instance.
        ::Core* EngineCore;

        /// Stores the generated runtime initialization options.
        ::CoreInitializationOptions* EngineOptions;

        /// Stores the generated runtime platform info instance.
        ::PlatformInfo* EnginePlatformInfo;

        /// Stores the lightweight Nintendo 3DS 3D backend used during startup-scene materialization.
        Nintendo3DsStartupRenderManager3D* EngineRenderManager3D;

        /// Stores the lightweight Nintendo 3DS 2D backend used during startup-scene materialization.
        Nintendo3DsStartupRenderManager2D* EngineRenderManager2D;

        /// Stores the Nintendo 3DS startup input backend used during startup-scene materialization.
        Nintendo3DsStartupInputBackend* EngineInputBackend;

        /// Stores the top-screen render-target metadata used by camera-bound viewport layout.
        ::RenderTarget* TopScreenRenderTargetMetadata;

        /// Stores the bottom-screen render-target metadata used by camera-bound viewport layout.
        ::RenderTarget* BottomScreenRenderTargetMetadata;
#endif

        /// Initializes libctru, citro3d, citro2d, and the screen render targets.
        bool InitializeRenderer();

        /// Draws one frame to both visible screens.
        void PresentFrame();

        /// Attempts to load the packaged startup manifest and apply its colors when valid.
        void TryApplyStartupManifestColors();

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        /// Builds one runtime standard-platform-input configuration from the generated manifest entries.
        ::StandardPlatformInputConfiguration* BuildStandardPlatformInputConfiguration();

        /// Initializes generated core with the minimal platform metadata required for startup.
        void InitializeGeneratedCore();

        /// Loads the packaged startup scene named by the generated runtime manifest through the runtime scene catalog.
        void LoadStartupScene();

        /// Assigns top-screen and bottom-screen render-target metadata to the active scene cameras so viewport-owned layout resolves against the correct 3DS screen size.
        void AssignScreenRenderTargetsToSceneCameras();

        /// Resolves the screen-local render-target metadata that should drive one camera-bound viewport subtree.
        /// <param name="camera">Camera whose current screen band should be classified.</param>
        /// <returns>Render-target metadata that exposes the correct 3DS screen dimensions for the camera.</returns>
        ::RenderTarget* ResolveScreenRenderTargetForCamera(::ICamera* camera) const;

        /// Resolves the runtime scene id that owns one cooked startup-scene asset path.
        /// <param name="cookedRelativePath">Cooked-relative startup-scene asset path from the runtime startup manifest.</param>
        /// <returns>Stable runtime scene id registered for that cooked scene path.</returns>
        std::string ResolveStartupSceneId(const std::string& cookedRelativePath) const;

        /// Presents one persistent diagnostic screen until the application is closed.
        /// <param name="topScreenColor">Solid color shown on the top screen during the diagnostic hold.</param>
        /// <param name="bottomScreenColor">Solid color shown on the bottom screen during the diagnostic hold.</param>
        void HoldOnFailure(u32 topScreenColor, u32 bottomScreenColor);
#endif

        /// Shuts down the renderer stack in reverse initialization order.
        void ShutdownRenderer();
    };
}
