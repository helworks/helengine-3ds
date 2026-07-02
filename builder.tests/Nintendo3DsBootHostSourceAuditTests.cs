namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Audits the Nintendo 3DS boot host source so generated-core startup and startup-scene materialization stay explicitly wired.
/// </summary>
public class Nintendo3DsBootHostSourceAuditTests {
    /// <summary>
    /// Verifies the Nintendo 3DS boot host initializes generated core with the real Nintendo 3DS 3D renderer before startup-scene materialization.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreIsEnabled_initializesCoreWithReal3dRenderer() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"Core.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"CoreInitializationOptions.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"ICamera.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"ObjectManager.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"PlatformInfo.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"RenderTarget.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"RuntimeSceneCatalog.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"RuntimeSceneCatalogEntry.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"StandardPlatformActionBinding.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"StandardPlatformInputConfiguration.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_scene_catalog_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"runtime/runtime_standard_platform_input_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InitializeGeneratedCore();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::RuntimeSceneCatalog* BuildRuntimeSceneCatalog()", sourceCode, StringComparison.Ordinal);
        Assert.Contains("const HERuntimeSceneCatalogEntry* sceneEntries = he_runtime_scene_catalog_entries(&sceneCount);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("new ::RuntimeSceneCatalogEntry(sourceEntry.SceneId, sourceEntry.CookedRelativePath)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore = new Core();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions = EngineCore->get_InitializationOptions();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_ContentRootPath(\"romfs:\");", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_SceneCatalog(BuildRuntimeSceneCatalog());", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineOptions->set_StandardPlatformInputConfiguration(BuildStandardPlatformInputConfiguration());", sourceCode, StringComparison.Ordinal);
        Assert.Contains("#include \"platform/3ds/Nintendo3DsRenderManager3D.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D = new Nintendo3DsRenderManager3D();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D = new Nintendo3DsStartupRenderManager2D();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineInputBackend = new Nintendo3DsStartupInputBackend();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("const char* runtimePlatformVersion = he_get_runtime_platform_version();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EnginePlatformInfo = new PlatformInfo(\"3ds\", runtimePlatformVersion);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D->AddWindow(0, 400, 240);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("TopScreenRenderTargetMetadata = new RenderTarget();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("TopScreenRenderTargetMetadata->set_Width(400);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("BottomScreenRenderTargetMetadata = new RenderTarget();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("BottomScreenRenderTargetMetadata->set_Width(320);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Initialize(EngineRenderManager3D, EngineRenderManager2D, EngineInputBackend, EnginePlatformInfo, EngineOptions);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ActiveTopScreenColor = GeneratedCoreTopScreenColor;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ActiveBottomScreenColor = GeneratedCoreBottomScreenColor;", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS boot host loads the startup scene directly from RomFS through the generated runtime manifest.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreIsEnabled_loadsStartupSceneThroughSceneManagerAndRuntimeSceneCatalog() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"runtime/runtime_startup_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("LoadStartupScene();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("Nintendo3DsPackagedAssetLoader packagedAssetLoader(\"romfs:\");", sourceCode, StringComparison.Ordinal);
        Assert.Contains("const char* startupSceneRelativePath = he_get_runtime_startup_scene_relative_path();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("std::string startupSceneId = ResolveStartupSceneId(startupSceneRelativePath);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("AssignScreenRenderTargetsToSceneCameras();", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("RuntimeSceneLoadService sceneLoadService(", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ActiveTopScreenColor = StartupSceneTopScreenColor;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ActiveBottomScreenColor = StartupSceneBottomScreenColor;", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS boot host stores generated-core state, startup-scene state, and checkpoint colors behind the compile-time generated-core seam.
    /// </summary>
    [Fact]
    public void Header_whenGeneratedCoreIsEnabled_declaresCoreStateAndCheckpointColors() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string headerPath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsBootHost.hpp");
        string headerCode = File.ReadAllText(headerPath);

        Assert.Contains("#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE", headerCode, StringComparison.Ordinal);
        Assert.Contains("#include <string>", headerCode, StringComparison.Ordinal);
        Assert.Contains("class Core;", headerCode, StringComparison.Ordinal);
        Assert.Contains("class CoreInitializationOptions;", headerCode, StringComparison.Ordinal);
        Assert.Contains("class ICamera;", headerCode, StringComparison.Ordinal);
        Assert.Contains("class PlatformInfo;", headerCode, StringComparison.Ordinal);
        Assert.Contains("class RenderTarget;", headerCode, StringComparison.Ordinal);
        Assert.Contains("static constexpr u32 GeneratedCoreTopScreenColor", headerCode, StringComparison.Ordinal);
        Assert.Contains("static constexpr u32 GeneratedCoreBottomScreenColor", headerCode, StringComparison.Ordinal);
        Assert.Contains("static constexpr u32 StartupSceneTopScreenColor", headerCode, StringComparison.Ordinal);
        Assert.Contains("static constexpr u32 StartupSceneBottomScreenColor", headerCode, StringComparison.Ordinal);
        Assert.Contains("::Core* EngineCore;", headerCode, StringComparison.Ordinal);
        Assert.Contains("::CoreInitializationOptions* EngineOptions;", headerCode, StringComparison.Ordinal);
        Assert.Contains("::PlatformInfo* EnginePlatformInfo;", headerCode, StringComparison.Ordinal);
        Assert.Contains("class Nintendo3DsRenderManager3D;", headerCode, StringComparison.Ordinal);
        Assert.Contains("Nintendo3DsRenderManager3D* EngineRenderManager3D;", headerCode, StringComparison.Ordinal);
        Assert.Contains("Nintendo3DsStartupRenderManager2D* EngineRenderManager2D;", headerCode, StringComparison.Ordinal);
        Assert.Contains("Nintendo3DsStartupInputBackend* EngineInputBackend;", headerCode, StringComparison.Ordinal);
        Assert.Contains("::RenderTarget* TopScreenRenderTargetMetadata;", headerCode, StringComparison.Ordinal);
        Assert.Contains("::RenderTarget* BottomScreenRenderTargetMetadata;", headerCode, StringComparison.Ordinal);
        Assert.Contains("::StandardPlatformInputConfiguration* BuildStandardPlatformInputConfiguration();", headerCode, StringComparison.Ordinal);
        Assert.Contains("void InitializeGeneratedCore();", headerCode, StringComparison.Ordinal);
        Assert.Contains("void LoadStartupScene();", headerCode, StringComparison.Ordinal);
        Assert.Contains("void AssignScreenRenderTargetsToSceneCameras();", headerCode, StringComparison.Ordinal);
        Assert.Contains("::RenderTarget* ResolveScreenRenderTargetForCamera(::ICamera* camera) const;", headerCode, StringComparison.Ordinal);
        Assert.Contains("std::string ResolveStartupSceneId(const std::string& cookedRelativePath) const;", headerCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS boot host advances generated core, captures 3D work, and presents the queued 3D pass before the 2D overlays every frame.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreIsEnabled_updatesCoreAndPresentsQueued3dBefore2dOverlays() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("while (aptMainLoop())", sourceCode, StringComparison.Ordinal);
        Assert.Contains("AssignScreenRenderTargetsToSceneCameras();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Update(1.0 / 60.0);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D->BeginFrame();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineCore->Draw();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->BeginFrame();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->Draw();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager3D->RenderTopScreen(TopTarget);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("topScreenClearColor = EngineRenderManager2D->ResolveTopScreenClearColor(topScreenClearColor);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("bottomScreenClearColor = EngineRenderManager2D->ResolveBottomScreenClearColor(bottomScreenClearColor);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->RenderTopScreen();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("EngineRenderManager2D->RenderBottomScreen();", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the mixed citro3d plus citro2d frame path re-prepares citro2d before each screen scene begins so textured 2D draws restore the expected GPU state after the 3D pass.
    /// </summary>
    [Fact]
    public void Source_whenPresentingMixed3dAnd2dFrame_repreparesCitro2dBeforeEachScreenScene() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        int topPrepareIndex = sourceCode.IndexOf("C2D_Prepare();", StringComparison.Ordinal);
        int topSceneBeginIndex = sourceCode.IndexOf("C2D_SceneBegin(TopTarget);", StringComparison.Ordinal);
        int bottomPrepareIndex = sourceCode.LastIndexOf("C2D_Prepare();", StringComparison.Ordinal);
        int bottomSceneBeginIndex = sourceCode.IndexOf("C2D_SceneBegin(BottomTarget);", StringComparison.Ordinal);

        Assert.True(topPrepareIndex >= 0 && topPrepareIndex < topSceneBeginIndex, "Expected C2D_Prepare before the top-screen 2D scene begins.");
        Assert.True(bottomPrepareIndex >= 0 && bottomPrepareIndex < bottomSceneBeginIndex, "Expected C2D_Prepare before the bottom-screen 2D scene begins.");
    }

    /// <summary>
    /// Verifies the boot host releases startup-owned generated-core objects so backend allocations do not leak across shutdown.
    /// </summary>
    [Fact]
    public void Source_whenRendererShutsDown_releasesGeneratedCoreStartupObjects() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsBootHost.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("delete EngineInputBackend;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("delete EngineRenderManager2D;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("delete EngineRenderManager3D;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("delete EnginePlatformInfo;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("delete EngineCore;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("delete BottomScreenRenderTargetMetadata;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("delete TopScreenRenderTargetMetadata;", sourceCode, StringComparison.Ordinal);
    }
}
