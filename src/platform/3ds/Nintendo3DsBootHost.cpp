#include "platform/3ds/Nintendo3DsBootHost.hpp"

#include <cstdio>
#include <exception>
#include <stdexcept>

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
#include "Asset.hpp"
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "EngineBinaryReadContext.hpp"
#include "ICamera.hpp"
#include "ObjectManager.hpp"
#include "PlatformInfo.hpp"
#include "RenderTarget.hpp"
#include "RuntimeSceneAssetReferenceResolver.hpp"
#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "RuntimeSceneLoadService.hpp"
#include "SceneLoadMode.hpp"
#include "SceneManager.hpp"
#include "StandardPlatformActionBinding.hpp"
#include "StandardPlatformInputConfiguration.hpp"
#include "platform/3ds/Nintendo3DsPackagedAssetLoader.hpp"
#include "platform/3ds/Nintendo3DsStartupInputBackend.hpp"
#include "platform/3ds/Nintendo3DsStartupRenderManager2D.hpp"
#include "platform/3ds/Nintendo3DsRenderManager3D.hpp"
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_standard_platform_input_manifest.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#include "runtime/native_list.hpp"
#include "runtime/native_exceptions.hpp"
#endif

namespace helengine::nintendo3ds {
    namespace {
        /// Stores the Nintendo 3DS top-screen pixel height used by the explicit Citro3D render target.
        constexpr int Nintendo3DsTopScreenPixelHeight = 240;

        /// Stores the Nintendo 3DS top-screen pixel width used by the explicit Citro3D render target.
        constexpr int Nintendo3DsTopScreenPixelWidth = 400;

        /// Stores the Nintendo 3DS bottom-screen pixel height used by the explicit Citro3D render target.
        constexpr int Nintendo3DsBottomScreenPixelHeight = 240;

        /// Stores the Nintendo 3DS bottom-screen pixel width used by the explicit Citro3D render target.
        constexpr int Nintendo3DsBottomScreenPixelWidth = 320;

        /// Stores the display-transfer flags recommended by devkitPro Citro3D samples for the top-screen 3D target.
        constexpr uint32_t Nintendo3DsTopScreenDisplayTransferFlags =
            GX_TRANSFER_FLIP_VERT(0)
            | GX_TRANSFER_OUT_TILED(0)
            | GX_TRANSFER_RAW_COPY(0)
            | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)
            | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)
            | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

        /// Stores the display-transfer flags used by the explicit Citro3D bottom-screen target.
        constexpr uint32_t Nintendo3DsBottomScreenDisplayTransferFlags =
            GX_TRANSFER_FLIP_VERT(0)
            | GX_TRANSFER_OUT_TILED(0)
            | GX_TRANSFER_RAW_COPY(0)
            | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)
            | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)
            | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

    }

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
    namespace {
        /// Stores the stable SD-card diagnostic log path used by the Nintendo 3DS boot host.
        constexpr const char* Nintendo3DsDiagnosticLogPath = "sdmc:/helengine_3ds_last_error.txt";

        /// Stores the stable SD-card live scene-trace log path used by the Nintendo 3DS boot host.
        constexpr const char* Nintendo3DsSceneTraceLogPath = "sdmc:/helengine_3ds_scene_trace.txt";

        /// Writes one startup or frame-loop diagnostic message to the emulated SD card for post-crash inspection.
        /// <param name="phase">Stable boot-host phase label that identified the failing boundary.</param>
        /// <param name="message">Best-effort exception message captured from the failing boundary.</param>
        void WriteDiagnosticLog(const char* phase, const char* message) {
            std::FILE* file = std::fopen(Nintendo3DsDiagnosticLogPath, "wb");
            if (file == nullptr) {
                return;
            }

            const char* resolvedPhase = phase != nullptr ? phase : "unknown";
            const char* resolvedMessage = message != nullptr ? message : "Unknown diagnostic message.";
            std::fprintf(file, "phase=%s\nmessage=%s\n", resolvedPhase, resolvedMessage);
            std::fclose(file);
        }

        /// Writes one live scene-trace snapshot to the emulated SD card for non-exception runtime debugging.
        /// <param name="message">Scene-trace payload to persist.</param>
        void WriteSceneTraceLog(const std::string& message) {
            std::FILE* file = std::fopen(Nintendo3DsSceneTraceLogPath, "wb");
            if (file == nullptr) {
                return;
            }

            std::fwrite(message.c_str(), 1, message.size(), file);
            std::fclose(file);
        }

        /// Appends one labeled diagnostic field when the supplied value contains meaningful content.
        /// <param name="message">Mutable message buffer that receives the formatted field.</param>
        /// <param name="label">Stable field label written into the diagnostic output.</param>
        /// <param name="value">Diagnostic value to append when non-empty.</param>
        void AppendDiagnosticField(std::string& message, const char* label, const std::string& value) {
            if (label == nullptr || label[0] == '\0' || value.empty()) {
                return;
            }

            message += "\n";
            message += label;
            message += "=";
            message += value;
        }

        /// Builds one expanded diagnostic message that overlays the managed runtime read context and scene-load trace state.
        /// <param name="baseMessage">Primary exception message captured by the 3DS boot host catch boundary.</param>
        /// <param name="engineCore">Generated-core instance that may expose additional runtime diagnostics.</param>
        /// <returns>Expanded diagnostic message with managed runtime context appended when available.</returns>
        std::string BuildManagedRuntimeDiagnosticMessage(const char* baseMessage, ::Core* engineCore) {
            std::string message = baseMessage != nullptr ? baseMessage : "Unknown diagnostic message.";
            AppendDiagnosticField(message, "binaryAssetPath", ::EngineBinaryReadContext::get_CurrentAssetPath());
            AppendDiagnosticField(message, "binaryReadStage", ::EngineBinaryReadContext::get_CurrentReadStage());
            AppendDiagnosticField(message, "binaryLastCheckpoint", ::EngineBinaryReadContext::get_LastCheckpoint());
            if (engineCore == nullptr) {
                return message;
            }

            ::RuntimeSceneLoadService* sceneLoadService = engineCore->get_SceneLoadService();
            if (sceneLoadService != nullptr) {
                AppendDiagnosticField(message, "sceneLoadStage", sceneLoadService->get_LastTraceStage());
                AppendDiagnosticField(message, "sceneLoadComponent", sceneLoadService->get_LastTraceComponentTypeId());
                AppendDiagnosticField(message, "textLoadStage", sceneLoadService->get_LastTextLoadStage());
                AppendDiagnosticField(message, "textFontPath", sceneLoadService->get_LastTextFontRelativePath());
                AppendDiagnosticField(message, "textureLoadStage", sceneLoadService->get_LastTextureLoadStage());
                AppendDiagnosticField(message, "texturePath", sceneLoadService->get_LastTextureRelativePath());
                AppendDiagnosticField(message, "fontDeserializeStage", sceneLoadService->get_LastFontDeserializeStage());
            }

            ::RuntimeSceneAssetReferenceResolver* referenceResolver = engineCore->get_SceneAssetReferenceResolver();
            if (referenceResolver != nullptr) {
                AppendDiagnosticField(message, "resolverTextLoadStage", referenceResolver->get_LastTextLoadStage());
                AppendDiagnosticField(message, "resolverTextFontPath", referenceResolver->get_LastTextFontRelativePath());
                AppendDiagnosticField(message, "resolverTextureLoadStage", referenceResolver->get_LastTextureLoadStage());
                AppendDiagnosticField(message, "resolverTexturePath", referenceResolver->get_LastTextureRelativePath());
                AppendDiagnosticField(message, "resolverFontDeserializeStage", referenceResolver->get_LastFontDeserializeStage());
            }

            return message;
        }

        /// Builds one live scene-trace snapshot that summarizes the current scene-manager state during normal execution.
        /// <param name="engineCore">Generated-core instance that exposes scene-manager diagnostics.</param>
        /// <returns>Formatted scene-trace snapshot.</returns>
        std::string BuildLiveSceneTraceMessage(::Core* engineCore) {
            if (engineCore == nullptr) {
                return "engineCore=<null>";
            }

            std::string message = "updateStage=" + engineCore->get_LastSceneTransitionStage();
            ::SceneManager* sceneManager = engineCore->get_SceneManager();
            if (sceneManager == nullptr) {
                message += "\nsceneManager=<null>";
                return message;
            }

            message += "\nsceneTransitionStage=" + sceneManager->get_LastTraceStage();
            message += "\nsceneId=" + sceneManager->get_LastTraceSceneId();
            message += "\nloadedSceneCount=" + std::to_string(sceneManager->get_LastTraceLoadedSceneCount());
            message += "\npendingSceneOperationCount=" + std::to_string(sceneManager->get_LastTracePendingOperationCount());

            List<std::string>* loadedSceneIds = sceneManager->GetLoadedSceneIds();
            if (loadedSceneIds == nullptr) {
                message += "\nloadedSceneIds=<null>";
                return message;
            }

            message += "\nloadedSceneIds=";
            for (int32_t index = 0; index < loadedSceneIds->get_Count(); index++) {
                if (index > 0) {
                    message += ",";
                }

                message += (*loadedSceneIds).get_Item(index);
            }

            delete loadedSceneIds;
            return message;
        }

        /// Persists the live scene trace only when the scene-manager state changes so the SD card log remains readable.
        /// <param name="engineCore">Generated-core instance whose scene state should be recorded.</param>
        void WriteLiveSceneTraceIfChanged(::Core* engineCore) {
            static std::string previousMessage;

            const std::string message = BuildLiveSceneTraceMessage(engineCore);
            if (message == previousMessage) {
                return;
            }

            previousMessage = message;
            WriteSceneTraceLog(message);
        }

        /// Builds one runtime scene catalog from the generated native scene-manifest entries.
        ::RuntimeSceneCatalog* BuildRuntimeSceneCatalog() {
            std::size_t sceneCount = 0;
            const HERuntimeSceneCatalogEntry* sceneEntries = he_runtime_scene_catalog_entries(&sceneCount);
            if (sceneEntries == nullptr || sceneCount == 0) {
                return nullptr;
            }

            Array<::RuntimeSceneCatalogEntry*>* catalogEntries = new Array<::RuntimeSceneCatalogEntry*>(static_cast<int32_t>(sceneCount));
            for (std::size_t index = 0; index < sceneCount; index++) {
                const HERuntimeSceneCatalogEntry& sourceEntry = sceneEntries[index];
                (*catalogEntries)[static_cast<int32_t>(index)] = new ::RuntimeSceneCatalogEntry(sourceEntry.SceneId, sourceEntry.CookedRelativePath);
            }

            return new ::RuntimeSceneCatalog(catalogEntries);
        }
    }
#endif

    /// Creates the Nintendo 3DS bootstrap host with no initialized render targets.
    Nintendo3DsBootHost::Nintendo3DsBootHost()
        : TopTarget(nullptr)
        , BottomTarget(nullptr)
        , ActiveTopScreenColor(TopScreenColor)
        , ActiveBottomScreenColor(BottomScreenColor)
        , StartupManifestReader()
        , RomFsInitialized(false)
#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        , EngineCore(nullptr)
        , EngineOptions(nullptr)
        , EnginePlatformInfo(nullptr)
        , EngineRenderManager3D(nullptr)
        , EngineRenderManager2D(nullptr)
        , EngineInputBackend(nullptr)
        , TopScreenRenderTargetMetadata(nullptr)
        , BottomScreenRenderTargetMetadata(nullptr)
#endif
    {
    }

    /// Initializes the Nintendo 3DS renderer stack and presents the startup scene until shutdown.
    int Nintendo3DsBootHost::Run() {
        if (!InitializeRenderer()) {
            ShutdownRenderer();
            return 1;
        }

        TryApplyStartupManifestColors();

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        try {
            InitializeGeneratedCore();
        } catch (const std::exception& exception) {
            WriteDiagnosticLog("generated-core-init", exception.what());
            HoldOnFailure(GeneratedCoreFailureTopScreenColor, GeneratedCoreFailureBottomScreenColor);
            ShutdownRenderer();
            return 1;
        } catch (const Exception* exception) {
            WriteDiagnosticLog("generated-core-init", exception != nullptr ? exception->what() : "Unknown managed runtime exception.");
            delete exception;
            HoldOnFailure(GeneratedCoreFailureTopScreenColor, GeneratedCoreFailureBottomScreenColor);
            ShutdownRenderer();
            return 1;
        } catch (...) {
            WriteDiagnosticLog("generated-core-init", "Unknown exception.");
            HoldOnFailure(GeneratedCoreFailureTopScreenColor, GeneratedCoreFailureBottomScreenColor);
            ShutdownRenderer();
            return 1;
        }

        try {
            LoadStartupScene();
        } catch (const std::exception& exception) {
            WriteDiagnosticLog("startup-scene-load", exception.what());
            HoldOnFailure(StartupSceneFailureTopScreenColor, StartupSceneFailureBottomScreenColor);
            ShutdownRenderer();
            return 1;
        } catch (const Exception* exception) {
            WriteDiagnosticLog("startup-scene-load", exception != nullptr ? exception->what() : "Unknown managed runtime exception.");
            delete exception;
            HoldOnFailure(StartupSceneFailureTopScreenColor, StartupSceneFailureBottomScreenColor);
            ShutdownRenderer();
            return 1;
        } catch (...) {
            WriteDiagnosticLog("startup-scene-load", "Unknown exception.");
            HoldOnFailure(StartupSceneFailureTopScreenColor, StartupSceneFailureBottomScreenColor);
            ShutdownRenderer();
            return 1;
        }
#endif

        while (aptMainLoop()) {
            hidScanInput();
#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
            if (EngineCore != nullptr && EngineRenderManager2D != nullptr && EngineRenderManager3D != nullptr) {
                try {
                    AssignScreenRenderTargetsToSceneCameras();
                    EngineRenderManager3D->BeginFrame();
                    EngineRenderManager2D->BeginFrame();
                    EngineCore->Update(1.0 / 60.0);
                    WriteLiveSceneTraceIfChanged(EngineCore);
                    EngineRenderManager2D->FlushReleasedTextures();
                } catch (const std::exception& exception) {
                    const std::string diagnosticMessage = BuildManagedRuntimeDiagnosticMessage(exception.what(), EngineCore);
                    WriteDiagnosticLog("core-update", diagnosticMessage.c_str());
                    HoldOnFailure(UpdateFailureTopScreenColor, UpdateFailureBottomScreenColor);
                    break;
                } catch (const Exception* exception) {
                    const std::string diagnosticMessage = BuildManagedRuntimeDiagnosticMessage(
                        exception != nullptr ? exception->what() : "Unknown managed runtime exception.",
                        EngineCore);
                    WriteDiagnosticLog("core-update", diagnosticMessage.c_str());
                    delete exception;
                    HoldOnFailure(UpdateFailureTopScreenColor, UpdateFailureBottomScreenColor);
                    break;
                } catch (...) {
                    WriteDiagnosticLog("core-update", "Unknown exception.");
                    HoldOnFailure(UpdateFailureTopScreenColor, UpdateFailureBottomScreenColor);
                    break;
                }

                try {
                    EngineCore->Draw();
                    EngineRenderManager2D->Draw();
                } catch (const std::exception& exception) {
                    const std::string diagnosticMessage = BuildManagedRuntimeDiagnosticMessage(exception.what(), EngineCore);
                    WriteDiagnosticLog("core-draw", diagnosticMessage.c_str());
                    HoldOnFailure(DrawFailureTopScreenColor, DrawFailureBottomScreenColor);
                    break;
                } catch (const Exception* exception) {
                    const std::string diagnosticMessage = BuildManagedRuntimeDiagnosticMessage(
                        exception != nullptr ? exception->what() : "Unknown managed runtime exception.",
                        EngineCore);
                    WriteDiagnosticLog("core-draw", diagnosticMessage.c_str());
                    delete exception;
                    HoldOnFailure(DrawFailureTopScreenColor, DrawFailureBottomScreenColor);
                    break;
                } catch (...) {
                    WriteDiagnosticLog("core-draw", "Unknown exception.");
                    HoldOnFailure(DrawFailureTopScreenColor, DrawFailureBottomScreenColor);
                    break;
                }
            }
#endif
            PresentFrame();
        }

        ShutdownRenderer();
        return 0;
    }

    /// Initializes libctru, citro3d, citro2d, and the screen render targets.
    bool Nintendo3DsBootHost::InitializeRenderer() {
        gfxInitDefault();
        RomFsInitialized = R_SUCCEEDED(romfsInit());

        if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
            return false;
        }

        if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
            return false;
        }

        C2D_Prepare();

        TopTarget = C3D_RenderTargetCreate(
            Nintendo3DsTopScreenPixelHeight,
            Nintendo3DsTopScreenPixelWidth,
            GPU_RB_RGBA8,
            GPU_RB_DEPTH24_STENCIL8);
        if (TopTarget != nullptr) {
            C3D_RenderTargetSetOutput(
                TopTarget,
                GFX_TOP,
                GFX_LEFT,
                Nintendo3DsTopScreenDisplayTransferFlags);
        }

        BottomTarget = C3D_RenderTargetCreate(
            Nintendo3DsBottomScreenPixelHeight,
            Nintendo3DsBottomScreenPixelWidth,
            GPU_RB_RGBA8,
            GPU_RB_DEPTH24_STENCIL8);
        if (BottomTarget != nullptr) {
            C3D_RenderTargetSetOutput(
                BottomTarget,
                GFX_BOTTOM,
                GFX_LEFT,
                Nintendo3DsBottomScreenDisplayTransferFlags);
        }

        if (TopTarget == nullptr) {
            return false;
        } else if (BottomTarget == nullptr) {
            return false;
        }

        return true;
    }

    /// Draws one frame to both visible screens and overlays any captured startup-scene 2D content on the top screen.
    void Nintendo3DsBootHost::PresentFrame() {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        u32 topScreenClearColor = ActiveTopScreenColor;
        u32 bottomScreenClearColor = ActiveBottomScreenColor;
#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        if (EngineRenderManager2D != nullptr) {
            topScreenClearColor = EngineRenderManager2D->ResolveTopScreenClearColor(topScreenClearColor);
            bottomScreenClearColor = EngineRenderManager2D->ResolveBottomScreenClearColor(bottomScreenClearColor);
        }
#endif
#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        if (EngineRenderManager3D != nullptr) {
            EngineRenderManager3D->RenderTopScreen(TopTarget, topScreenClearColor);
        }
#else
        C3D_RenderTargetClear(TopTarget, C3D_CLEAR_ALL, topScreenClearColor, 0);
#endif
        C3D_FrameSplit(0);
        C2D_SceneBegin(TopTarget);
#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        if (EngineRenderManager2D != nullptr) {
            EngineRenderManager2D->RenderTopScreen();
        }
#endif
        C2D_Flush();
        C3D_FrameSplit(0);
        C2D_TargetClear(BottomTarget, bottomScreenClearColor);
        C2D_SceneBegin(BottomTarget);
#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        if (EngineRenderManager2D != nullptr) {
            EngineRenderManager2D->RenderBottomScreen();
        }
#endif
        C2D_Flush();

        C3D_FrameEnd(0);
    }

    /// Attempts to load the packaged startup manifest and apply its colors when valid.
    void Nintendo3DsBootHost::TryApplyStartupManifestColors() {
        Nintendo3DsStartupManifestReader::Result result = StartupManifestReader.Read(RomFsInitialized);
        if (result.ReadStatus != Nintendo3DsStartupManifestReader::Status::Success) {
            return;
        }

        ActiveTopScreenColor = result.TopScreenColor;
        ActiveBottomScreenColor = result.BottomScreenColor;
    }

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
    /// Builds one runtime standard-platform-input configuration from the generated manifest entries.
    ::StandardPlatformInputConfiguration* Nintendo3DsBootHost::BuildStandardPlatformInputConfiguration() {
        std::size_t entryCount = 0;
        const HERuntimeStandardPlatformActionEntry* actionEntries = he_runtime_standard_platform_action_entries(&entryCount);
        List<::StandardPlatformActionBinding*>* bindings = new List<::StandardPlatformActionBinding*>();
        for (std::size_t index = 0; index < entryCount; index++) {
            const HERuntimeStandardPlatformActionEntry& sourceEntry = actionEntries[index];
            ::InputControlId controlId(
                static_cast<::InputDeviceKind>(sourceEntry.DeviceKind),
                static_cast<::InputControlKind>(sourceEntry.ControlKind),
                sourceEntry.DeviceIndex,
                sourceEntry.ControlIndex);
            bindings->Add(new ::StandardPlatformActionBinding(
                static_cast<::StandardPlatformAction>(sourceEntry.ActionId),
                controlId));
        }

        return new ::StandardPlatformInputConfiguration(bindings);
    }

    /// Initializes generated core with the minimal platform metadata required for startup.
    void Nintendo3DsBootHost::InitializeGeneratedCore() {
        EngineCore = new Core();
        EngineOptions = EngineCore->get_InitializationOptions();
        EngineOptions->set_ContentRootPath("romfs:");
        EngineOptions->set_SceneCatalog(BuildRuntimeSceneCatalog());
        EngineOptions->set_StandardPlatformInputConfiguration(BuildStandardPlatformInputConfiguration());
        EngineOptions->set_UpdateOrderLayers(4);
        EngineOptions->set_RenderOrderLayers3D(4);
        EngineOptions->set_UpdateListInitialCapacity(64);
        EngineOptions->set_RenderList2DInitialCapacity(64);
        EngineOptions->set_RenderList3DInitialCapacity(64);

        EngineRenderManager3D = new Nintendo3DsRenderManager3D();
        EngineRenderManager2D = new Nintendo3DsStartupRenderManager2D();
        EngineInputBackend = new Nintendo3DsStartupInputBackend();
        const char* runtimePlatformVersion = he_get_runtime_platform_version();
        EnginePlatformInfo = new PlatformInfo("3ds", runtimePlatformVersion);
        EngineRenderManager3D->AddWindow(0, 400, 240);
        TopScreenRenderTargetMetadata = new RenderTarget();
        TopScreenRenderTargetMetadata->set_Width(400);
        TopScreenRenderTargetMetadata->set_Height(240);
        BottomScreenRenderTargetMetadata = new RenderTarget();
        BottomScreenRenderTargetMetadata->set_Width(320);
        BottomScreenRenderTargetMetadata->set_Height(240);
        EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo, EngineOptions);

        ActiveTopScreenColor = GeneratedCoreTopScreenColor;
        ActiveBottomScreenColor = GeneratedCoreBottomScreenColor;
    }

    /// Loads the packaged startup scene named by the generated runtime manifest through the runtime scene catalog.
    void Nintendo3DsBootHost::LoadStartupScene() {
        Nintendo3DsPackagedAssetLoader packagedAssetLoader("romfs:");
        const char* startupSceneRelativePath = he_get_runtime_startup_scene_relative_path();
        if (startupSceneRelativePath == nullptr || startupSceneRelativePath[0] == '\0') {
            WriteDiagnosticLog("startup-scene-load", "Runtime startup scene path was missing.");
            return;
        } else if (!packagedAssetLoader.AssetExists(startupSceneRelativePath)) {
            WriteDiagnosticLog("startup-scene-load", startupSceneRelativePath);
            return;
        }

        std::string startupSceneId = ResolveStartupSceneId(startupSceneRelativePath);
        EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);
        AssignScreenRenderTargetsToSceneCameras();

        ActiveTopScreenColor = StartupSceneTopScreenColor;
        ActiveBottomScreenColor = StartupSceneBottomScreenColor;
    }

    /// Assigns top-screen and bottom-screen render-target metadata to the active scene cameras so viewport-owned layout resolves against the correct 3DS screen size.
    void Nintendo3DsBootHost::AssignScreenRenderTargetsToSceneCameras() {
        if (EngineCore == nullptr || EngineCore->get_ObjectManager() == nullptr) {
            return;
        } else if (TopScreenRenderTargetMetadata == nullptr || BottomScreenRenderTargetMetadata == nullptr) {
            return;
        }

        List<ICamera*>* cameras = EngineCore->get_ObjectManager()->get_Cameras();
        if (cameras == nullptr) {
            return;
        }

        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            ICamera* camera = (*cameras)[cameraIndex];
            if (camera == nullptr) {
                continue;
            }

            camera->set_RenderTarget(ResolveScreenRenderTargetForCamera(camera));
        }
    }

    /// Resolves the screen-local render-target metadata that should drive one camera-bound viewport subtree.
    /// <param name="camera">Camera whose current screen band should be classified.</param>
    /// <returns>Render-target metadata that exposes the correct 3DS screen dimensions for the camera.</returns>
    RenderTarget* Nintendo3DsBootHost::ResolveScreenRenderTargetForCamera(ICamera* camera) const {
        if (camera == nullptr) {
            throw std::invalid_argument("Nintendo 3DS camera classification requires one camera.");
        }

        float4 viewport = camera->get_Viewport();
        if (viewport.Z <= 1.0f && viewport.W <= 1.0f) {
            return viewport.Y >= 1.0f
                ? BottomScreenRenderTargetMetadata
                : TopScreenRenderTargetMetadata;
        }

        return viewport.Y >= 240.0f
            ? BottomScreenRenderTargetMetadata
            : TopScreenRenderTargetMetadata;
    }

    /// Resolves the runtime scene id that owns one cooked startup-scene asset path.
    /// <param name="cookedRelativePath">Cooked-relative startup-scene asset path from the runtime startup manifest.</param>
    /// <returns>Stable runtime scene id registered for that cooked scene path.</returns>
    std::string Nintendo3DsBootHost::ResolveStartupSceneId(const std::string& cookedRelativePath) const {
        if (cookedRelativePath.empty()) {
            throw std::invalid_argument("Nintendo 3DS startup scene path is required.");
        } else if (EngineOptions == nullptr || EngineOptions->get_SceneCatalog() == nullptr) {
            throw std::runtime_error("Nintendo 3DS startup scene resolution requires an initialized runtime scene catalog.");
        }

        Array<::RuntimeSceneCatalogEntry*>* sceneCatalogEntries = EngineOptions->get_SceneCatalog()->get_Entries();
        if (sceneCatalogEntries == nullptr) {
            throw std::runtime_error("Nintendo 3DS runtime scene catalog entries were unavailable during startup scene resolution.");
        }

        for (int32_t index = 0; index < sceneCatalogEntries->Length; index++) {
            ::RuntimeSceneCatalogEntry* sceneCatalogEntry = (*sceneCatalogEntries)[index];
            if (sceneCatalogEntry == nullptr) {
                continue;
            } else if (sceneCatalogEntry->get_CookedRelativePath() != cookedRelativePath) {
                continue;
            }

            return sceneCatalogEntry->get_SceneId();
        }

        throw std::runtime_error(std::string("Nintendo 3DS runtime scene catalog did not contain startup scene path '") + cookedRelativePath + std::string("'."));
    }

    /// Presents one persistent diagnostic screen until the application is closed.
    /// <param name="topScreenColor">Solid color shown on the top screen during the diagnostic hold.</param>
    /// <param name="bottomScreenColor">Solid color shown on the bottom screen during the diagnostic hold.</param>
    void Nintendo3DsBootHost::HoldOnFailure(u32 topScreenColor, u32 bottomScreenColor) {
        ActiveTopScreenColor = topScreenColor;
        ActiveBottomScreenColor = bottomScreenColor;

        while (aptMainLoop()) {
            hidScanInput();
            PresentFrame();
        }
    }
#endif

    /// Shuts down the renderer stack in reverse initialization order.
    void Nintendo3DsBootHost::ShutdownRenderer() {
#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE
        delete EngineInputBackend;
        EngineInputBackend = nullptr;
        delete EngineRenderManager2D;
        EngineRenderManager2D = nullptr;
        delete EngineRenderManager3D;
        EngineRenderManager3D = nullptr;
        delete EnginePlatformInfo;
        EnginePlatformInfo = nullptr;
        delete EngineCore;
        EngineCore = nullptr;
        EngineOptions = nullptr;
        delete BottomScreenRenderTargetMetadata;
        BottomScreenRenderTargetMetadata = nullptr;
        delete TopScreenRenderTargetMetadata;
        TopScreenRenderTargetMetadata = nullptr;
#endif
        C2D_Fini();
        C3D_Fini();
        if (RomFsInitialized) {
            romfsExit();
            RomFsInitialized = false;
        }
        gfxExit();
    }
}
