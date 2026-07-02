# 3DS Top-Screen Mixed Scene Authoring Design

## Goal

Prove that normal HelEngine scene authoring on Nintendo 3DS supports top-screen 3D and top-screen 2D content in the same frame without platform-specific debug injection.

The first milestone uses `cube_test_ds` as the proof scene. The scene must keep its rotating unlit cube and add one authored top-screen text element that renders as `Hello World`.

## Scope

- Keep `cube_test_ds` as the active proof scene.
- Add one normal top-screen 2D text drawable authored through the engine scene path.
- Preserve the existing rotating unlit cube on the top screen.
- Do not use boot-host-owned debug text.
- Do not inject fake platform-owned draw commands.
- Do not redesign the scene beyond the minimum content needed to prove mixed authoring.

## Non-Goals

- Lit 3D rendering or material expansion.
- General UI system improvements beyond what is required for the proof scene.
- Scene-authoring changes unrelated to `cube_test_ds`.
- Bottom-screen behavior changes unless they are required to keep the frame path correct.

## Recommended Approach

Use `cube_test_ds` as an ordinary mixed-authored scene:

1. Author top-screen 3D content normally through the existing cube scene.
2. Author top-screen 2D text normally through the existing scene/runtime path.
3. Keep the 3DS platform responsible only for correct capture, target routing, and replay ordering.

This approach proves the intended engine behavior instead of proving a platform-specific fallback.

## Architecture

### Scene Layer

`cube_test_ds` gains one normal text drawable configured to target the top screen and render `Hello World`.

The scene remains the source of truth for mixed content. The text must be authored the same way other engine text content is authored, using standard scene/runtime data rather than a 3DS-only special case.

### 3DS Runtime Layer

The current mixed frame path remains responsible for correct execution:

1. Capture 3D work from generated core.
2. Replay top-screen 3D through Citro3D.
3. Replay top-screen 2D through Citro2D on the same top render target.
4. Replay bottom-screen 2D on the bottom target.

No boot-host-owned debug text or scene-specific renderer hooks are added.

### Responsibility Boundary

- Scene content proves normal authoring.
- 3DS runtime proves target routing and frame ordering.
- Tests prove the source-level contract for replay order.

## Data Flow

1. The generated core updates `cube_test_ds`.
2. The scene's camera and drawable setup produces a normal 3D queue and a normal 2D queue.
3. `Nintendo3DsRenderManager3D` captures and replays top-screen 3D.
4. `Nintendo3DsStartupRenderManager2D` captures and replays top-screen text on the top target.
5. The final top screen shows both the rotating cube and `Hello World`.

## Failure Modes

### Incorrect Top-Screen 2D Routing

Symptoms:
- `Hello World` appears on the bottom screen.
- `Hello World` does not appear at all.

Likely causes:
- Camera viewport classification is wrong.
- Top-screen 2D replay is not targeting the top render target correctly.

### Render-State Regression

Symptoms:
- `Hello World` appears but the cube regresses.
- The next frame loses correct 3D rendering after top-screen 2D playback.

Likely causes:
- Citro2D mutates GPU state that is not fully restored before the next top-screen 3D frame.

## Testing

### Source-Level Verification

Retain focused 3DS source-audit coverage for the mixed frame path so the runtime still guarantees:

- top-screen 3D replay occurs before top-screen 2D replay
- top-screen 2D replay targets the top render target
- bottom-screen 2D replay remains isolated to the bottom target

### Runtime Verification

Rebuild the 3DS artifact with `cube_test_ds` and launch it in Azahar.

Success criteria:

- the rotating white unlit cube still renders on the top screen
- authored top-screen text renders as `Hello World`
- both are visible in the same running scene

## Increment

This milestone proves one narrow but important guarantee: normal authored top-screen 3D and top-screen 2D can coexist in the same 3DS scene without special-case platform drawing.
