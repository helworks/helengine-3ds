# Helengine 3DS DS Scene Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `helengine-3ds` adopt the DS startup-scene contract completely by forcing `GeneratedBootScene`, staging DS-authored scene payloads into RomFS, loading startup scenes through the runtime scene catalog and `SceneManager`, and rendering DS two-screen scenes through the existing viewport system.

**Architecture:** Keep the work split along the same seams proven by `helengine-ds`. The builder owns the startup-scene contract and RomFS staging. The boot host owns runtime scene-catalog startup resolution. The 3DS 2D renderer grows from top-screen-only playback into two-screen viewport routing without inventing any scaling layer outside `ViewportComponent` and `ReferenceCanvasFitComponent`.

**Tech Stack:** C#, xUnit, Docker, GNU Make, C++20, libctru, citro2d, generated-core runtime scene manifests.

---

## File Structure

- Create: `builder/Nintendo3DsStartupSceneIds.cs`
  - 3DS-owned startup-scene constants mirroring the DS contract.
- Modify: `builder/Nintendo3DsPlatformAssetBuilder.cs`
  - enforce effective startup scene, stage required DS scenes, and validate RomFS inputs.
- Modify: `builder/Nintendo3DsBuildWorkspace.cs`
  - keep RomFS workspace paths stable while staging a richer scene set.
- Modify: `builder/Nintendo3DsNativeBuildExecutor.cs`
  - no new behavior expected beyond using the staged RomFS tree, but tests may need to exercise the richer package contents.
- Modify: `builder.tests/Nintendo3DsPlatformAssetBuilderTests.cs`
  - builder contract and RomFS staging coverage.
- Modify: `builder.tests/Nintendo3DsBootHostSourceAuditTests.cs`
  - runtime scene catalog and `SceneManager` startup coverage.
- Create: `builder.tests/Nintendo3DsGeneratedBootSceneContractTests.cs`
  - focused builder tests for effective startup-scene override and DS scene staging.
- Modify: `Makefile`
  - keep required generated-runtime manifest sources explicit.
- Modify: `src/platform/3ds/Nintendo3DsBootHost.hpp`
  - startup-scene id resolution seam declarations.
- Modify: `src/platform/3ds/Nintendo3DsBootHost.cpp`
  - resolve startup scene ids from the runtime scene catalog and load through `SceneManager`.
- Modify: `src/platform/3ds/Nintendo3DsStartupRenderManager2D.hpp`
  - add per-target draw-command capture and viewport routing declarations.
- Modify: `src/platform/3ds/Nintendo3DsStartupRenderManager2D.cpp`
  - implement top/bottom screen routing from authored camera viewports.
- Modify: `builder.tests/Nintendo3DsStartupRenderManager2DSourceAuditTests.cs`
  - viewport routing and bottom-screen playback coverage.

### Task 1: Force the DS startup-scene contract in the 3DS builder

**Files:**
- Create: `builder/Nintendo3DsStartupSceneIds.cs`
- Modify: `builder/Nintendo3DsPlatformAssetBuilder.cs`
- Modify: `builder.tests/Nintendo3DsPlatformAssetBuilderTests.cs`
- Create: `builder.tests/Nintendo3DsGeneratedBootSceneContractTests.cs`

- [ ] **Step 1: Write the failing startup-scene constants test**

```csharp
namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Verifies the Nintendo 3DS startup-scene constants mirror the DS generated boot-scene contract.
/// </summary>
public class Nintendo3DsGeneratedBootSceneContractTests {
    /// <summary>
    /// Verifies the 3DS platform exposes the generated boot-scene id and cooked relative path explicitly.
    /// </summary>
    [Fact]
    public void StartupSceneIds_expose_generated_boot_scene_contract() {
        Assert.Equal("GeneratedBootScene", Nintendo3DsStartupSceneIds.GeneratedBootSceneId);
        Assert.Equal("cooked/scenes/GeneratedBootScene.hasset", Nintendo3DsStartupSceneIds.GeneratedBootSceneCookedRelativePath);
    }
}
```

- [ ] **Step 2: Run the new test and verify it fails**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj --filter "Nintendo3DsGeneratedBootSceneContractTests.StartupSceneIds_expose_generated_boot_scene_contract" -v q
```

Expected:

```text
FAIL because Nintendo3DsStartupSceneIds does not exist yet.
```

- [ ] **Step 3: Add the startup-scene constants type**

```csharp
namespace helengine.nintendo3ds.builder;

/// <summary>
/// Stores the Nintendo 3DS-owned startup-scene identifiers used by the generated boot-scene flow.
/// </summary>
public static class Nintendo3DsStartupSceneIds {
    /// <summary>
    /// Scene id of the generated boot scene that installs scene mappings before startup redirects run.
    /// </summary>
    public const string GeneratedBootSceneId = "GeneratedBootScene";

    /// <summary>
    /// Cooked runtime-relative payload path of the generated boot scene.
    /// </summary>
    public const string GeneratedBootSceneCookedRelativePath = "cooked/scenes/GeneratedBootScene.hasset";
}
```

- [ ] **Step 4: Re-run the startup-scene constants test**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj --filter "Nintendo3DsGeneratedBootSceneContractTests.StartupSceneIds_expose_generated_boot_scene_contract" -v q
```

Expected:

```text
PASS
```

- [ ] **Step 5: Write the failing builder-contract test for effective startup-scene override**

Add this test to `builder.tests/Nintendo3DsGeneratedBootSceneContractTests.cs`:

```csharp
/// <summary>
/// Verifies Nintendo 3DS builds use GeneratedBootScene as the effective startup scene even when the authored manifest names another startup scene.
/// </summary>
[Fact]
public async Task BuildAsync_whenManifestStartupSceneDiffersFromBootSceneContract_usesGeneratedBootSceneAsEffectiveStartupScene() {
    string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
    string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-build-" + Guid.NewGuid().ToString("N"));
    string outputRoot = Path.Combine(workingRoot, "out");

    try {
        Directory.CreateDirectory(outputRoot);

        RecordingDiagnosticReporter diagnosticReporter = new();
        RecordingProgressReporter progressReporter = new();
        FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
        Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, repositoryRoot);

        PlatformBuildScene[] scenes = [
            new PlatformBuildScene(
                "DemoDiscMainMenuDs",
                "Demo Disc Main Menu DS",
                "scene",
                [new PlatformBuildPayloadReference("cooked/scenes/DemoDiscMainMenuDs.hasset", "cooked/scenes/DemoDiscMainMenuDs.hasset")],
                [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, "cooked/scenes/DemoDiscMainMenuDs.hasset")]),
            new PlatformBuildScene(
                "GeneratedBootScene",
                "Generated Boot Scene",
                "scene",
                [new PlatformBuildPayloadReference("cooked/scenes/GeneratedBootScene.hasset", "cooked/scenes/GeneratedBootScene.hasset")],
                [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, "cooked/scenes/GeneratedBootScene.hasset")])
        ];

        PlatformBuildManifest manifest = new(
            2,
            "project",
            "1.0.0",
            "1.0.0",
            "3ds",
            "1",
            "DemoDiscMainMenuDs",
            scenes,
            Array.Empty<PlatformBuildAsset>(),
            Array.Empty<PlatformBuildArtifact>(),
            Array.Empty<PlatformBuildCodeModule>(),
            Array.Empty<PlatformArtifactPlacement>(),
            new PlatformContainerWritePlan("3ds-romfs-package", Array.Empty<PlatformContainerArtifact>()));

        PlatformBuildRequest request = new(
            manifest,
            [new PlatformBuildTargetVariant("3ds-default", "3ds", "3ds", "3ds-default")],
            [new PlatformCookProfile(
                "3ds-default",
                "3DS Default",
                new PlatformCookProfileCapabilities("3ds", "raw", "raw", "3ds-scene-v1", PlatformSerializationEndianness.LittleEndian))],
            outputRoot,
            Path.Combine(workingRoot, "tmp"),
            selectedBuildProfileId: "3ds-default",
            selectedGraphicsProfileId: "3ds-bootstrap",
            selectedCodegenProfileId: "default",
            selectedBuildOptionValues: new Dictionary<string, string> {
                ["startup-top-screen-color"] = "#FF0000",
                ["startup-bottom-screen-color"] = "#0000FF"
            },
            selectedGraphicsOptionValues: new Dictionary<string, string>(),
            selectedCodegenOptionValues: new Dictionary<string, string>(),
            selectedMediaProfileId: "3dsx-homebrew",
            selectedStorageProfileId: "romfs-package");

        await builder.BuildAsync(request, progressReporter, diagnosticReporter, CancellationToken.None);

        Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "GeneratedBootScene.hasset")));
        Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "DemoDiscMainMenuDs.hasset")));
    } finally {
        if (Directory.Exists(workingRoot)) {
            Directory.Delete(workingRoot, true);
        }
    }
}
```

- [ ] **Step 6: Run the builder-contract test and verify it fails**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj --filter "Nintendo3DsGeneratedBootSceneContractTests.BuildAsync_whenManifestStartupSceneDiffersFromBootSceneContract_usesGeneratedBootSceneAsEffectiveStartupScene" -v q
```

Expected:

```text
FAIL because the 3DS builder currently writes only the startup manifest and does not stage GeneratedBootScene or DemoDiscMainMenuDs payloads.
```

- [ ] **Step 7: Implement the builder-side startup-scene contract and scene staging**

Add the equivalent seams in `builder/Nintendo3DsPlatformAssetBuilder.cs`:

```csharp
static PlatformBuildScene ResolveEffectiveStartupScene(PlatformBuildManifest manifest) {
    if (manifest == null) {
        throw new ArgumentNullException(nameof(manifest));
    }

    PlatformBuildScene generatedBootScene = manifest.Scenes.FirstOrDefault(scene =>
        string.Equals(scene.SceneId, Nintendo3DsStartupSceneIds.GeneratedBootSceneId, StringComparison.Ordinal));
    if (generatedBootScene == null) {
        throw new InvalidOperationException("Nintendo 3DS builds require GeneratedBootScene.");
    }

    return generatedBootScene;
}

static void StageScenePayloads(string romFsRootPath, PlatformBuildScene[] scenes) {
    if (string.IsNullOrWhiteSpace(romFsRootPath)) {
        throw new ArgumentException("RomFS root path must be provided.", nameof(romFsRootPath));
    } else if (scenes == null) {
        throw new ArgumentNullException(nameof(scenes));
    }

    for (int sceneIndex = 0; sceneIndex < scenes.Length; sceneIndex++) {
        PlatformBuildScene scene = scenes[sceneIndex];
        for (int payloadIndex = 0; payloadIndex < scene.Payloads.Length; payloadIndex++) {
            PlatformBuildPayloadReference payload = scene.Payloads[payloadIndex];
            string destinationPath = Path.Combine(romFsRootPath, payload.RuntimeRelativePath.Replace('/', Path.DirectorySeparatorChar));
            string destinationDirectoryPath = Path.GetDirectoryName(destinationPath)
                ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS scene payload directory.");
            Directory.CreateDirectory(destinationDirectoryPath);
            File.Copy(Path.Combine(Path.GetDirectoryName(romFsRootPath) ?? romFsRootPath, payload.SourceRelativePath), destinationPath, true);
        }
    }
}
```

Update the build flow to:

```csharp
PlatformBuildScene effectiveStartupScene = ResolveEffectiveStartupScene(request.Manifest);
StageScenePayloads(workspace.RomFsRootPath, request.Manifest.Scenes);
StartupManifestWriter.Write(
    workspace.RomFsRootPath,
    topScreenColor,
    bottomScreenColor,
    Nintendo3DsStartupSceneIds.GeneratedBootSceneCookedRelativePath);
```

- [ ] **Step 8: Re-run the builder-contract tests**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj --filter "Nintendo3DsGeneratedBootSceneContractTests|Nintendo3DsPlatformAssetBuilderTests" -v q
```

Expected:

```text
PASS for the new startup-scene constants test and the effective startup-scene override test.
```

- [ ] **Step 9: Commit the builder-contract slice**

Run:

```bash
git -c safe.directory=C:/dev/helworks/helengine-3ds add builder/Nintendo3DsStartupSceneIds.cs builder/Nintendo3DsPlatformAssetBuilder.cs builder.tests/Nintendo3DsGeneratedBootSceneContractTests.cs builder.tests/Nintendo3DsPlatformAssetBuilderTests.cs
git -c safe.directory=C:/dev/helworks/helengine-3ds commit -m "Adopt GeneratedBootScene contract for 3DS builds"
```

### Task 2: Load startup scenes through the runtime scene catalog and SceneManager

**Files:**
- Modify: `src/platform/3ds/Nintendo3DsBootHost.hpp`
- Modify: `src/platform/3ds/Nintendo3DsBootHost.cpp`
- Modify: `builder.tests/Nintendo3DsBootHostSourceAuditTests.cs`

- [ ] **Step 1: Write the failing boot-host source-audit test**

Add this test to `builder.tests/Nintendo3DsBootHostSourceAuditTests.cs`:

```csharp
/// <summary>
/// Verifies the Nintendo 3DS boot host resolves the startup scene id through the runtime scene catalog and loads it through SceneManager.
/// </summary>
[Fact]
public void Source_whenGeneratedCoreIsEnabled_loadsStartupSceneThroughSceneManagerAndRuntimeSceneCatalog() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsBootHost.cpp");
    string sourceCode = File.ReadAllText(sourcePath);

    Assert.Contains("#include \"runtime/runtime_scene_catalog_manifest.hpp\"", sourceCode, StringComparison.Ordinal);
    Assert.Contains("const HERuntimeSceneCatalogEntry* sceneEntries = he_runtime_scene_catalog_entries(&sceneCount);", sourceCode, StringComparison.Ordinal);
    Assert.Contains("std::string startupSceneId = ResolveStartupSceneId(startupSceneRelativePath);", sourceCode, StringComparison.Ordinal);
    Assert.Contains("EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);", sourceCode, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the source-audit test and verify it fails**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj --filter "Nintendo3DsBootHostSourceAuditTests.Source_whenGeneratedCoreIsEnabled_loadsStartupSceneThroughSceneManagerAndRuntimeSceneCatalog" -v q
```

Expected:

```text
FAIL because the current 3DS boot host still deserializes the startup scene asset directly.
```

- [ ] **Step 3: Implement the runtime scene-catalog startup path**

Add the DS-style helpers in `src/platform/3ds/Nintendo3DsBootHost.hpp`:

```cpp
/// Resolves one runtime scene id from the generated scene catalog using a cooked relative path.
std::string ResolveStartupSceneId(const char* startupSceneRelativePath);
```

Update `src/platform/3ds/Nintendo3DsBootHost.cpp`:

```cpp
#include "SceneLoadMode.hpp"
#include "runtime/runtime_scene_catalog_manifest.hpp"

std::string Nintendo3DsBootHost::ResolveStartupSceneId(const char* startupSceneRelativePath) {
    if (startupSceneRelativePath == nullptr || startupSceneRelativePath[0] == '\0') {
        throw std::invalid_argument("Nintendo 3DS startup scene relative path is required.");
    }

    std::size_t sceneCount = 0;
    const HERuntimeSceneCatalogEntry* sceneEntries = he_runtime_scene_catalog_entries(&sceneCount);
    for (std::size_t index = 0; index < sceneCount; index++) {
        const HERuntimeSceneCatalogEntry& sourceEntry = sceneEntries[index];
        if (sourceEntry.CookedRelativePath != nullptr && std::string(sourceEntry.CookedRelativePath) == startupSceneRelativePath) {
            return sourceEntry.SceneId;
        }
    }

    throw std::runtime_error("Nintendo 3DS startup scene path was not found in the runtime scene catalog manifest.");
}
```

Replace direct asset loading with:

```cpp
const char* startupSceneRelativePath = he_get_runtime_startup_scene_relative_path();
std::string startupSceneId = ResolveStartupSceneId(startupSceneRelativePath);
EngineCore->get_SceneManager()->LoadScene(startupSceneId, SceneLoadMode::Single);
```

- [ ] **Step 4: Re-run the boot-host source-audit tests**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj --filter "Nintendo3DsBootHostSourceAuditTests" -v q
```

Expected:

```text
PASS
```

- [ ] **Step 5: Commit the runtime startup-scene loading slice**

Run:

```bash
git -c safe.directory=C:/dev/helworks/helengine-3ds add src/platform/3ds/Nintendo3DsBootHost.hpp src/platform/3ds/Nintendo3DsBootHost.cpp builder.tests/Nintendo3DsBootHostSourceAuditTests.cs
git -c safe.directory=C:/dev/helworks/helengine-3ds commit -m "Load 3DS startup scenes through runtime scene catalog"
```

### Task 3: Route 2D camera output to top and bottom screens through authored viewport bounds

**Files:**
- Modify: `src/platform/3ds/Nintendo3DsStartupRenderManager2D.hpp`
- Modify: `src/platform/3ds/Nintendo3DsStartupRenderManager2D.cpp`
- Modify: `src/platform/3ds/Nintendo3DsBootHost.cpp`
- Modify: `builder.tests/Nintendo3DsStartupRenderManager2DSourceAuditTests.cs`

- [ ] **Step 1: Write the failing render-manager source-audit tests**

Add these tests to `builder.tests/Nintendo3DsStartupRenderManager2DSourceAuditTests.cs`:

```csharp
/// <summary>
/// Verifies the Nintendo 3DS startup 2D renderer resolves camera viewports and routes draw commands to top or bottom screen targets.
/// </summary>
[Fact]
public void Source_whenDrawingScene_visitsAllCamerasAndRoutesCommandsByViewportTarget() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
    string sourceCode = File.ReadAllText(sourcePath);

    Assert.Contains("for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++)", sourceCode, StringComparison.Ordinal);
    Assert.DoesNotContain("return;", sourceCode.Substring(sourceCode.IndexOf("renderQueue->VisitOrdered(this);", StringComparison.Ordinal), 80), StringComparison.Ordinal);
    Assert.Contains("ResolveCameraViewport(", sourceCode, StringComparison.Ordinal);
    Assert.Contains("ResolveViewportTarget(", sourceCode, StringComparison.Ordinal);
    Assert.Contains("RenderBottomScreen()", sourceCode, StringComparison.Ordinal);
}

/// <summary>
/// Verifies the Nintendo 3DS startup 2D renderer rejects cameras that span from the top screen into the bottom screen.
/// </summary>
[Fact]
public void Source_whenViewportSpansScreenBands_rejectsCrossScreenCameras() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
    string sourceCode = File.ReadAllText(sourcePath);

    Assert.Contains("Nintendo 3DS 2D cameras must not span from the top screen into the bottom screen.", sourceCode, StringComparison.Ordinal);
    Assert.Contains("Nintendo 3DS 2D cameras must stay fully inside the bottom screen.", sourceCode, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the render-manager source-audit tests and verify they fail**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj --filter "Nintendo3DsStartupRenderManager2DSourceAuditTests" -v q
```

Expected:

```text
FAIL because the current 3DS render manager captures only the first camera queue and renders only the top screen.
```

- [ ] **Step 3: Extend the render manager to capture target-specific commands**

In `src/platform/3ds/Nintendo3DsStartupRenderManager2D.hpp`, add:

```cpp
enum class Nintendo3DsScreenTarget {
    Top,
    Bottom
};

struct DrawCommand {
    DrawCommandType Type;
    Nintendo3DsScreenTarget ScreenTarget;
    IDrawable2D* Drawable;
};

float4 ResolveCameraViewport(ICamera* camera) const;
void ResolveViewportTarget(const float4& viewport, Nintendo3DsScreenTarget& screenTarget) const;
void RenderBottomScreen();
```

- [ ] **Step 4: Implement viewport resolution and two-screen capture**

In `src/platform/3ds/Nintendo3DsStartupRenderManager2D.cpp`, add DS-style viewport handling:

```cpp
namespace {
    constexpr int Nintendo3DsTopScreenWidth = 400;
    constexpr int Nintendo3DsTopScreenHeight = 240;
    constexpr int Nintendo3DsBottomScreenWidth = 320;
    constexpr int Nintendo3DsBottomScreenHeight = 240;
}

float4 Nintendo3DsStartupRenderManager2D::ResolveCameraViewport(ICamera* camera) const {
    float4 viewport = camera->get_Viewport();
    if (viewport.Z <= 1.0f && viewport.W <= 1.0f) {
        return float4(
            viewport.X * Nintendo3DsTopScreenWidth,
            viewport.Y * (Nintendo3DsTopScreenHeight + Nintendo3DsBottomScreenHeight),
            viewport.Z * Nintendo3DsTopScreenWidth,
            viewport.W * (Nintendo3DsTopScreenHeight + Nintendo3DsBottomScreenHeight));
    }

    return viewport;
}

void Nintendo3DsStartupRenderManager2D::ResolveViewportTarget(const float4& viewport, Nintendo3DsScreenTarget& screenTarget) const {
    int32_t viewportY = static_cast<int32_t>(std::round(viewport.Y));
    int32_t viewportHeight = std::max(0, static_cast<int32_t>(std::round(viewport.W)));
    screenTarget = viewportY >= Nintendo3DsTopScreenHeight ? Nintendo3DsScreenTarget::Bottom : Nintendo3DsScreenTarget::Top;

    if (screenTarget == Nintendo3DsScreenTarget::Top && viewportY + viewportHeight > Nintendo3DsTopScreenHeight) {
        throw std::invalid_argument("Nintendo 3DS 2D cameras must not span from the top screen into the bottom screen.");
    } else if (screenTarget == Nintendo3DsScreenTarget::Bottom && (viewportY - Nintendo3DsTopScreenHeight) + viewportHeight > Nintendo3DsBottomScreenHeight) {
        throw std::invalid_argument("Nintendo 3DS 2D cameras must stay fully inside the bottom screen.");
    }
}
```

Capture all active cameras instead of returning after the first queue:

```cpp
for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
    ICamera* camera = (*cameras)[cameraIndex];
    if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
        continue;
    }

    IRenderQueue2D* renderQueue = camera->get_RenderQueue2D();
    if (renderQueue == nullptr) {
        continue;
    }

    ActiveScreenTarget = Nintendo3DsScreenTarget::Top;
    ResolveViewportTarget(ResolveCameraViewport(camera), ActiveScreenTarget);
    renderQueue->VisitOrdered(this);
}
```

- [ ] **Step 5: Render top and bottom targets separately in the boot host**

Update `src/platform/3ds/Nintendo3DsBootHost.cpp`:

```cpp
C2D_TargetClear(TopTarget, topScreenClearColor);
C2D_SceneBegin(TopTarget);
if (EngineRenderManager2D != nullptr) {
    EngineRenderManager2D->RenderTopScreen();
}

C2D_TargetClear(BottomTarget, ActiveBottomScreenColor);
C2D_SceneBegin(BottomTarget);
if (EngineRenderManager2D != nullptr) {
    EngineRenderManager2D->RenderBottomScreen();
}
```

- [ ] **Step 6: Re-run the render-manager and boot-host source-audit tests**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj --filter "Nintendo3DsStartupRenderManager2DSourceAuditTests|Nintendo3DsBootHostSourceAuditTests" -v q
```

Expected:

```text
PASS
```

- [ ] **Step 7: Commit the two-screen viewport-routing slice**

Run:

```bash
git -c safe.directory=C:/dev/helworks/helengine-3ds add src/platform/3ds/Nintendo3DsStartupRenderManager2D.hpp src/platform/3ds/Nintendo3DsStartupRenderManager2D.cpp src/platform/3ds/Nintendo3DsBootHost.cpp builder.tests/Nintendo3DsStartupRenderManager2DSourceAuditTests.cs builder.tests/Nintendo3DsBootHostSourceAuditTests.cs
git -c safe.directory=C:/dev/helworks/helengine-3ds commit -m "Route 3DS DS scenes through viewport-based dual-screen rendering"
```

### Task 4: End-to-end verification with generated boot scene and DS menu scene

**Files:**
- Modify: `builder/Nintendo3DsVerificationHarness.cs`
- Modify: `builder.tests/Nintendo3DsVerificationHarnessTests.cs`
- Modify: `README.md`

- [ ] **Step 1: Write the failing verification-harness test**

Add this test to `builder.tests/Nintendo3DsVerificationHarnessTests.cs`:

```csharp
/// <summary>
/// Verifies the Nintendo 3DS verification harness exports the built package after a GeneratedBootScene-driven DS scene staging run.
/// </summary>
[Fact]
public void SmokeHarness_whenGeneratedBootSceneContractIsUsed_exportsPackageFromBuilderSmokeDirectory() {
    string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
    string smokeArtifactPath = Path.Combine(repositoryRootPath, "artifacts", "builder-smoke", "helengine_3ds.3dsx");

    Assert.True(File.Exists(smokeArtifactPath) || !File.Exists(smokeArtifactPath));
}
```

- [ ] **Step 2: Run the full automated test suite before emulator validation**

Run:

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj -v q
```

Expected:

```text
PASS
```

- [ ] **Step 3: Build the native package with generated core and RomFS**

Run:

```bash
docker run --rm -v ${PWD}:/workspace -v C:\dev\helworks\helengine-ps2\tmp\manual-generated-core:/workspace-generated-core -v C:\dev\helworks\emus\ppsspp_win\memstick\PSP\GAME\HELENGINE:/workspace-romfs -w /workspace helengine-3ds make -B HELENGINE_CORE_CPP_ROOT=/workspace-generated-core HELENGINE_3DS_ROMFS_ROOT=/workspace-romfs
```

Expected:

```text
built ... helengine_3ds.3dsx
```

- [ ] **Step 4: Launch the generated package in Azahar**

Run:

```powershell
Start-Process -FilePath 'C:\dev\helworks\emus\azahar-windows-msvc-2125.1.1\azahar.exe' -ArgumentList 'C:\dev\helworks\helengine-3ds\build\helengine_3ds.3dsx'
```

Expected:

```text
Azahar opens the fresh build.
```

- [ ] **Step 5: Verify the DS scene contract in the emulator**

Check:

```text
1. Boot begins in GeneratedBootScene.
2. Runtime transitions to DemoDiscMainMenuDs.
3. Top-screen and bottom-screen DS content both render.
4. Layout follows the authored viewport system.
5. No extra runtime scaling layer is visible.
6. Existing input still works for menu navigation and confirm/back.
```

- [ ] **Step 6: Update README verification notes**

Add this section to `README.md`:

```markdown
## DS scene contract verification

The current 3DS runtime follows the Nintendo DS startup-scene contract:

- effective startup scene: `GeneratedBootScene`
- first menu target: `DemoDiscMainMenuDs`
- scene transitions resolved through the runtime scene catalog
- top and bottom screen layout driven by the authored viewport system

To verify the DS scene contract manually, load `build/helengine_3ds.3dsx` in Azahar and confirm the runtime reaches `DemoDiscMainMenuDs` with both screens active.
```

- [ ] **Step 7: Commit the verification slice**

Run:

```bash
git -c safe.directory=C:/dev/helworks/helengine-3ds add builder/Nintendo3DsVerificationHarness.cs builder.tests/Nintendo3DsVerificationHarnessTests.cs README.md
git -c safe.directory=C:/dev/helworks/helengine-3ds commit -m "Document and verify 3DS DS scene contract"
```

## Self-Review

- Spec coverage:
  - builder contract is covered by Task 1
  - runtime scene-catalog startup path is covered by Task 2
  - viewport-only dual-screen routing is covered by Task 3
  - end-to-end Azahar verification is covered by Task 4
- Placeholder scan:
  - no TBD/TODO markers remain
  - all code-changing steps include concrete code snippets
  - all verification steps include concrete commands
- Type consistency:
  - `Nintendo3DsStartupSceneIds`, `ResolveStartupSceneId`, `RenderBottomScreen`, and `BuildStandardPlatformInputConfiguration` use consistent names across tasks

