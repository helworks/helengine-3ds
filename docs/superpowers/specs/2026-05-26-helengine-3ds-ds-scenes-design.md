# Helengine Nintendo 3DS DS Scene Contract Design

## Goal

Extend `helengine-3ds` from the current startup-scene proof into the DS-authored menu and playable-scene flow. The Nintendo 3DS platform should adopt the DS startup contract end to end: boot `GeneratedBootScene`, resolve scene-map targets into DS-authored scenes such as `DemoDiscMainMenuDs`, and stage the DS scene set into the 3DS RomFS package.

## Scope

This design covers the next 3DS vertical slice after generated-core initialization, startup-scene loading, 2D rendering, and input routing are already working.

Included:
- 3DS builder contract change to force `GeneratedBootScene` as the effective startup scene
- staging of DS-authored menu and playable scenes into the 3DS RomFS package
- 3DS runtime scene-catalog resolution for DS scene ids
- 3DS top-screen and bottom-screen 2D camera routing through authored viewport bounds
- viewport-faithful rendering of DS-authored two-screen scenes on 3DS

Excluded:
- new non-viewport scaling systems
- ad hoc screen-fit code outside the existing viewport system
- generalized 3D viewport reinterpretation beyond what the DS scene flow needs
- unrelated scene-authoring changes in the city project
- input behavior changes beyond preserving the current working mapping

## Repository Context

`helengine-3ds` already has:
- builder and builder tests
- RomFS packaging
- generated-core compile and initialization
- startup scene loading from packaged content
- startup 2D rendering with text, rounded rects, and texture-backed sprites
- startup input backend with standard-platform action wiring

`helengine-ds` already proves the target contract:
- builder forces `GeneratedBootScene` as the effective runtime startup scene
- runtime scene catalog is used to resolve scene ids to cooked relative paths
- `GeneratedBootScene` routes into DS-specific scene targets such as `DemoDiscMainMenuDs`
- DS runtime routes cameras to top or bottom screen from authored viewport placement

The 3DS work should reuse that contract and those seams, not invent a 3DS-only boot path.

## Approved Direction

Adopt the DS contract completely on 3DS.

The 3DS platform should:
- force `GeneratedBootScene` as the effective startup scene in the builder
- stage `GeneratedBootScene` plus DS-specific scene-map targets into RomFS
- load `GeneratedBootScene` first at runtime
- let generated-core scene-map logic route into `DemoDiscMainMenuDs` and DS playable scenes
- render DS-authored top and bottom screen content through the existing viewport system only

This avoids temporary runtime shortcuts and keeps the 3DS platform aligned with the data-driven boot flow already proven by DS.

## Architecture

### Builder Contract

The 3DS builder should mirror the DS startup-scene contract.

Required behavior:
- effective startup scene id becomes `GeneratedBootScene`
- effective startup cooked relative path becomes `cooked/scenes/GeneratedBootScene.hasset`
- the runtime startup manifest written for 3DS must point to that cooked path
- if the authored build manifest names another startup scene, 3DS must still use `GeneratedBootScene` as the effective startup scene, matching DS

The builder must stage:
- `GeneratedBootScene.hasset`
- `DemoDiscMainMenuDs.hasset`
- DS-mapped playable scene payloads that `GeneratedBootScene` can resolve into at runtime

The builder must also preserve the runtime scene catalog entries needed for generated-core `SceneMapComponent` resolution. The runtime should not depend on a 3DS-specific scene-id lookup shortcut.

### Runtime Boot Flow

The runtime boot flow should stay simple and data-driven:

1. initialize 3DS renderer and generated core
2. load the startup scene path from the runtime startup manifest
3. resolve the startup scene id through the runtime scene catalog
4. ask `SceneManager` to load that startup scene id
5. let `GeneratedBootScene` run its existing scene-map logic and transition to `DemoDiscMainMenuDs`

This means the 3DS runtime should follow the DS pattern more closely than the current direct asset-materialization path. Direct scene asset loading was the right bring-up seam, but for DS scene flow the correct seam is runtime `SceneManager` loading by scene id.

### Viewport and Screen Routing

The user constraint is explicit: do not add a scaling system outside the viewport system.

Therefore the 3DS implementation must honor:
- `ViewportComponent`
- `ReferenceCanvasFitComponent`
- authored camera viewport bounds

The 3DS renderer should not hardcode "scale DS screen to 3DS screen" logic as a separate transform layer. Instead, it should:
- inspect each active camera viewport
- resolve viewport bounds in authored scene space
- map top-band cameras to the top 3DS target
- map bottom-band cameras to the bottom 3DS target
- preserve the viewport-authored layout behavior that already drives scaling elsewhere in the engine

First-pass routing rules should match DS discipline:
- top-screen cameras must stay fully inside the top screen band
- bottom-screen cameras must stay fully inside the bottom screen band
- cameras that span from top to bottom should fail fast instead of being auto-corrected

This preserves authored intent and avoids hidden runtime transforms.

### Render Manager Growth

Current 3DS 2D rendering is top-screen-only. To support DS scenes, `Nintendo3DsStartupRenderManager2D` needs to grow in these areas:

- draw traversal must process all active cameras needed for the scene rather than stopping at the first 2D queue
- captured draw commands must remember which render target they belong to
- clear-color resolution must work per target when both screens are active
- final playback must render commands separately to top and bottom targets
- viewport resolution logic must come from camera viewport bounds, not from a post-layout screen fit

The current render manager can keep using citro2d for playback. The change is in camera traversal, viewport resolution, and target assignment.

## Components

### Builder-side

- `builder/Nintendo3DsPlatformAssetBuilder.cs`
  - apply the DS startup-scene contract
  - stage required DS boot-scene and DS menu-scene payloads
- new 3DS startup-scene constants type, parallel to DS
  - define `GeneratedBootScene` id and cooked path explicitly
- existing staged RomFS packaging path
  - include DS scene payloads and any required generated runtime manifests

### Runtime-side

- `src/platform/3ds/Nintendo3DsBootHost.cpp`
  - align startup-scene loading with runtime scene-catalog id resolution
  - use `SceneManager`-driven scene boot where the contract requires it
- `src/platform/3ds/Nintendo3DsStartupRenderManager2D.*`
  - expand from top-screen-only queue playback to two-screen viewport-aware rendering

### Test-side

- source-audit tests for:
  - 3DS startup contract matching DS contract
  - runtime scene catalog usage
  - two-screen viewport routing behavior
- builder tests for:
  - forced `GeneratedBootScene` startup behavior
  - required DS scene payload staging
  - RomFS inclusion of `GeneratedBootScene` and `DemoDiscMainMenuDs`

## Data Flow

### Build-time

1. editor/build manifest reaches the 3DS builder
2. builder computes the effective startup scene
3. builder overrides that effective startup scene to `GeneratedBootScene`
4. builder stages `GeneratedBootScene` and DS scene-map targets into RomFS
5. generated runtime manifests are staged with:
   - startup scene cooked relative path
   - runtime scene catalog
   - standard platform input manifest
6. native build packages RomFS into `.3dsx`

### Runtime

1. 3DS host boots and initializes generated core
2. runtime startup manifest yields `cooked/scenes/GeneratedBootScene.hasset`
3. runtime scene catalog resolves the scene id for that cooked path
4. `SceneManager` loads `GeneratedBootScene`
5. `GeneratedBootScene` resolves scene-map targets into DS scene ids
6. runtime transitions into `DemoDiscMainMenuDs`
7. render manager routes top/bottom camera content to the correct 3DS targets by viewport band

## Error Handling

Do not silently patch missing DS scene content.

3DS builds should fail when:
- `GeneratedBootScene` is not available in the build manifest inputs required by the contract
- the cooked payload for `GeneratedBootScene` is missing
- `DemoDiscMainMenuDs` or required DS target scenes are missing from staged content
- runtime scene catalog data is missing for required DS scene ids

Runtime should fail fast or stay on the current visible checkpoint when:
- startup scene cooked path is missing
- startup scene id cannot be resolved from the runtime scene catalog
- a camera viewport spans across screen bands

The design explicitly avoids "best effort" auto-correction for viewport misuse.

## Verification Plan

### Automated

- builder tests verify:
  - 3DS forces `GeneratedBootScene` as effective startup scene
  - `GeneratedBootScene.hasset` is staged into RomFS
  - `DemoDiscMainMenuDs.hasset` is staged into RomFS
  - runtime scene catalog manifests are included
- source-audit tests verify:
  - 3DS boot host uses runtime scene-catalog resolution
  - 3DS render manager routes content by camera viewport to top and bottom screens

### Native Build

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj -v q
docker run --rm -v ${PWD}:/workspace -v <generated-core>:/workspace-generated-core -v <romfs-root>:/workspace-romfs -w /workspace helengine-3ds make -B HELENGINE_CORE_CPP_ROOT=/workspace-generated-core HELENGINE_3DS_ROMFS_ROOT=/workspace-romfs
```

Expected result:
- tests pass
- `.3dsx` packages successfully

### Emulator Validation

Load the produced `.3dsx` in Azahar.

Success criteria:
- boot starts in `GeneratedBootScene`
- runtime transitions into `DemoDiscMainMenuDs`
- both screens render authored DS scene content
- layout/scaling follows the authored viewport system
- no separate screen-fit distortion path is visible

## Constraints

- the 3DS platform must adopt the DS startup-scene contract completely
- no scaling behavior may be added outside `ViewportComponent` / `ReferenceCanvasFitComponent`-driven behavior
- no 3DS-only hardcoded menu jump path
- no cross-screen camera auto-fix behavior
- changes must stay incremental and testable in small slices

## Recommended Implementation Order

1. builder contract slice
   - force `GeneratedBootScene`
   - stage required DS scenes
   - verify manifests and RomFS contents
2. runtime startup-scene resolution slice
   - use runtime scene catalog and `SceneManager`
3. two-screen viewport routing slice
   - top/bottom target assignment by authored viewport
4. Azahar validation slice
   - confirm `GeneratedBootScene -> DemoDiscMainMenuDs`

This order preserves small-step debugging while still moving to the full DS contract.
