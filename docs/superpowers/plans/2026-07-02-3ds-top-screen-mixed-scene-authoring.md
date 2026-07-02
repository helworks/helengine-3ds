# 3DS Top-Screen Mixed Scene Authoring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `cube_test_ds` prove normal mixed top-screen authoring by rendering the rotating unlit cube and one authored `Hello World` text element in the same scene.

**Architecture:** Keep the authored `Hello World` text in the shared generated `cube_test` scene roots so the existing Nintendo DS scaffold preserves it on the top screen automatically. Do not add boot-host debug text or 3DS-only draw injection; rely on the current 3DS mixed frame path and existing DS scaffold behavior.

**Tech Stack:** C#, HelEngine editor scene generation, city project scene factories, existing 3DS Citro3D/Citro2D runtime, xUnit source-audit tests, headless editor command runner

---

## File Structure

- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`
  - Extend the city authoring contract to require one generated top-screen `Hello World` text entity in `CubeTestSceneFactory`.
- Modify: `C:\dev\helprojs\city\assets\codebase\rendering.tools\CubeTestSceneFactory.cs`
  - Add one generated top-screen text entity and include it in the shared `RootEntities` so the DS scaffold preserves it automatically.
- Regenerate: `C:\dev\helprojs\city\assets\scenes\rendering\cube_test.helen`
  - Persist the updated shared generated cube-test scene.
- Regenerate: `C:\dev\helprojs\city\assets\scenes\rendering\ds\cube_test_ds.helen`
  - Persist the DS companion scene derived from the updated shared top-screen roots.
- Verify only: `C:\dev\helworks\helengine-3ds\builder.tests\Nintendo3DsBootHostSourceAuditTests.cs`
  - Existing 3DS mixed-frame audit stays green.
- Verify only: `C:\dev\helworks\helengine-3ds\builder.tests\Nintendo3DsRenderManager3DSourceAuditTests.cs`
  - Existing 3DS top-screen 3D replay audit stays green.

### Task 1: Lock The Mixed-Authoring Contract In Tests

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj`

- [ ] **Step 1: Write the failing city source test for the top-screen `Hello World` entity**

Add this test to `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`:

```csharp
/// <summary>
/// Ensures the generated cube-test scene authors one top-screen hello-world text entity that survives into the DS companion scene through the shared root path.
/// </summary>
[Fact]
public void City_cube_test_scene_source_authors_top_screen_hello_world_text() {
    string sourcePath = @"C:\dev\helprojs\city\assets\codebase\rendering.tools\CubeTestSceneFactory.cs";
    string source = File.ReadAllText(sourcePath);

    Assert.Contains("Entity topScreenHelloWorldEntity = CreateTopScreenHelloWorldEntity(instructionFont);", source, StringComparison.Ordinal);
    Assert.Contains("CreateTopScreenHelloWorldEntity(FontAsset font)", source, StringComparison.Ordinal);
    Assert.Contains("Text = \"Hello World\"", source, StringComparison.Ordinal);
    Assert.Contains("Font = font", source, StringComparison.Ordinal);
    Assert.Contains("FontScale = 1f", source, StringComparison.Ordinal);
    Assert.Contains("Size = new int2(128, 24)", source, StringComparison.Ordinal);
    Assert.Contains("RenderOrder2D = 180", source, StringComparison.Ordinal);
    Assert.Contains("entity.LocalPosition = new float3(12f, 12f, 0f);", source, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the filtered city source tests and verify the new test fails**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~CityCubeTestSceneSourceTests -v q
```

Expected:

```text
FAIL because CubeTestSceneFactory.cs does not yet contain CreateTopScreenHelloWorldEntity(...) or the "Hello World" text contract.
```

- [ ] **Step 3: Commit nothing yet**

Do not commit in the red state.

### Task 2: Author The Shared Top-Screen Text Entity And Regenerate The Scenes

**Files:**
- Modify: `C:\dev\helprojs\city\assets\codebase\rendering.tools\CubeTestSceneFactory.cs`
- Regenerate: `C:\dev\helprojs\city\assets\scenes\rendering\cube_test.helen`
- Regenerate: `C:\dev\helprojs\city\assets\scenes\rendering\ds\cube_test_ds.helen`
- Test: `C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj`

- [ ] **Step 1: Add the generated top-screen text entity to the shared cube-test scene roots**

Update `CreateSceneDefinition(...)` in `C:\dev\helprojs\city\assets\codebase\rendering.tools\CubeTestSceneFactory.cs` to create and include the new top-screen text entity:

```csharp
FontAsset instructionFont = ResolveRequiredEditorFont();
DemoSceneInstructionOverlayFactory instructionOverlayFactory = new DemoSceneInstructionOverlayFactory();
Entity cameraEntity = CreateCameraEntity();
Entity instructionOverlayEntity = instructionOverlayFactory.CreateDesktopInstructionOverlayRoot(instructionFont);
Entity topScreenHelloWorldEntity = CreateTopScreenHelloWorldEntity(instructionFont);

return new GeneratedAuthoringSceneDefinition {
    SceneId = SceneId,
    SceneSettings = new SceneSettingsAsset(),
    NintendoDsScene = new GeneratedDsSceneDefinition {
        SceneId = RenderingSceneGenerator.CubeTestNintendoDsSceneId,
        UseDefaultBottomOverlay = false,
        BottomScreenRootEntities = Array.Empty<Entity>()
    },
    RootEntities = new[] {
        cameraEntity,
        instructionOverlayEntity,
        CreateUiEntity(),
        topScreenHelloWorldEntity,
        CreateDirectionalLightEntity(),
        CreateCubeEntity(cubeModel, solidColorMaterial)
    }
};
```

Add this helper method to the same file:

```csharp
/// <summary>
/// Creates the authored top-screen hello-world text entity used to prove mixed 3D plus 2D scene authoring on Nintendo DS and 3DS companion scenes.
/// </summary>
/// <param name="font">Loaded editor font assigned to the authored text component.</param>
/// <returns>Live authored top-screen text entity.</returns>
Entity CreateTopScreenHelloWorldEntity(FontAsset font) {
    if (font == null) {
        throw new ArgumentNullException(nameof(font));
    }

    Entity entity = Core.Instance.EntityFactory.Create("CubeTestTopScreenHelloWorld");
    entity.LayerMask = EditorLayerMasks.SceneObjects;
    entity.LocalPosition = new float3(12f, 12f, 0f);
    entity.LocalScale = float3.One;
    entity.LocalOrientation = float4.Identity;
    entity.AddComponent(new TextComponent {
        Text = "Hello World",
        Font = font,
        FontScale = 1f,
        Color = new byte4(255, 255, 255, 255),
        Size = new int2(128, 24),
        RenderOrder2D = 180
    });
    return entity;
}
```

- [ ] **Step 2: Run the filtered city source tests and verify they pass**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~CityCubeTestSceneSourceTests -v q
```

Expected:

```text
PASS with all CityCubeTestSceneSourceTests green.
```

- [ ] **Step 3: Regenerate the rendering scenes through the headless editor command**

Run:

```powershell
rtk dotnet run --project C:\dev\helworks\helengine\helengine.ui\helengine.editor.app\helengine.editor.app.csproj -- --project C:\dev\helprojs\city\project.heproj --editor-command menu.generate-rendering-scenes
```

Expected:

```text
Editor command 'menu.generate-rendering-scenes' executed successfully.
```

- [ ] **Step 4: Verify the regenerated scene assets contain the authored text**

Run:

```powershell
rtk rg -a -n "Hello World|CubeTestTopScreenHelloWorld" C:\dev\helprojs\city\assets\scenes\rendering\cube_test.helen C:\dev\helprojs\city\assets\scenes\rendering\ds\cube_test_ds.helen
```

Expected:

```text
Both scene files contain the CubeTestTopScreenHelloWorld entity name and the Hello World text payload.
```

- [ ] **Step 5: Commit the authoring change and regenerated scenes**

Run:

```bash
git -C C:\dev\helworks\helengine add engine/helengine.editor.tests/CityCubeTestSceneSourceTests.cs
git -C C:\dev\helworks\helengine commit -m "test: require cube test hello world authoring"
git -C C:\dev\helprojs\city add assets/codebase/rendering.tools/CubeTestSceneFactory.cs assets/scenes/rendering/cube_test.helen assets/scenes/rendering/ds/cube_test_ds.helen
git -C C:\dev\helprojs\city commit -m "feat: author top-screen hello world for cube test ds"
```

### Task 3: Re-Verify The 3DS Mixed Frame Path Against The Authored Scene

**Files:**
- Verify only: `C:\dev\helworks\helengine-3ds\builder.tests\Nintendo3DsBootHostSourceAuditTests.cs`
- Verify only: `C:\dev\helworks\helengine-3ds\builder.tests\Nintendo3DsRenderManager3DSourceAuditTests.cs`
- Verify artifact: `C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx`

- [ ] **Step 1: Run the focused 3DS source-audit tests and verify they stay green**

Run:

```powershell
rtk dotnet test C:\dev\helworks\helengine-3ds\builder.tests\helengine.3ds.builder.tests.csproj --filter FullyQualifiedName~Nintendo3DsBootHostSourceAuditTests -v q
rtk dotnet test C:\dev\helworks\helengine-3ds\builder.tests\helengine.3ds.builder.tests.csproj --filter FullyQualifiedName~Nintendo3DsRenderManager3DSourceAuditTests -v q
```

Expected:

```text
PASS for both source-audit groups with zero failures.
```

- [ ] **Step 2: Build the 3DS artifact for `cube_test_ds`**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -Command "`$env:HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID='cube_test_ds'; & 'C:\dev\helworks\helengine\artifacts\build-platform.ps1' -Project 'C:\dev\helprojs\city\project.heproj' -Platform 3ds -Output 'C:\dev\helprojs\city\3ds-build'"
```

Expected:

```text
Build completed for platform '3ds': C:\dev\helprojs\city\3ds-build
```

- [ ] **Step 3: Launch the new 3DS artifact in Azahar**

Run:

```powershell
rtk powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-3ds\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx
```

Expected:

```text
ARTIFACT=C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx
PROCESS_ID=<pid>
```

- [ ] **Step 4: Verify the visual result manually**

Check in Azahar:

```text
The top screen shows both:
1. the rotating white unlit cube
2. the authored Hello World text
```

- [ ] **Step 5: Commit only if Task 2 was not committed yet**

If Task 2 was already committed after green regeneration, do not create a second no-op commit. If verification exposed a small fix and you changed code, commit that follow-up with:

```bash
git commit -m "fix: preserve mixed top-screen cube test authoring on 3ds"
```
