# 3DS Lit Cube Design

## Goal

Make the Nintendo 3DS `cube_test_ds` cube respond to the authored scene directional light like the other platform builds, while keeping the current top-screen 2D overlay and authored camera clear behavior intact.

## Current State

- The 3DS renderer currently draws 3D meshes through a solid-color path.
- The cube uses mesh positions only and ignores mesh normals.
- The shader path does not consume directional-light data.
- The scene already authors a directional light and a lit-looking material expectation on other platforms.

## Scope

In scope:

- Extend the 3DS runtime model path to preserve mesh normals.
- Add a minimal directional-light shading path for the 3DS top-screen 3D renderer.
- Resolve one active directional light from the authored scene and apply it to the cube-test mesh path.
- Keep the existing mixed 3D plus top-screen 2D frame path working.

Out of scope:

- Full standard-material parity with desktop platforms.
- Multiple dynamic lights, shadows, specular, normal maps, or post-processing.
- Broader material-system redesign.

## Recommended Approach

Implement a minimal Lambert-style directional-light renderer in the existing 3DS 3D path.

Reasons:

- It matches the user-visible goal at the correct seam.
- It keeps the 3DS renderer architecture simple.
- It avoids scene-authored hacks and avoids pretending unlit color is equivalent to lit material behavior.

## Design

### Runtime model data

- Preserve mesh normals when building `Nintendo3DsRuntimeModel`.
- Upload a packed vertex stream containing position and normal data.
- Keep submesh traversal and draw submission logic unchanged apart from the new vertex layout.

### Shader path

- Replace the current solid-color shader program with a minimal lit shader pair for the 3DS renderer.
- Vertex shader responsibilities:
  - transform position by model-view and projection
  - transform or pass through normal into view space
- Fragment or lighting stage responsibilities:
  - evaluate one directional-light Lambert term
  - combine diffuse light with a fixed material base color
  - clamp the result to a readable minimum so the cube remains visible

### Light resolution

- Capture one active authored directional light from the loaded scene.
- Feed light direction and light color/intensity into the shader uniforms each frame.
- Prefer the first enabled directional light that matches the top-screen scene content.

### Frame behavior

- Preserve the existing top-screen clear behavior.
- Preserve the existing top-screen `Hello World` overlay and bottom-screen UI routing.
- Avoid changes to scene authoring other than consuming the already-authored light correctly.

## Verification

- Rebuild 3DS with `HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID=cube_test_ds`.
- Launch in Azahar.
- Confirm:
  - top screen still uses the authored cornflower-blue clear
  - `Hello World` still appears only on the top screen
  - bottom-screen back button and text remain bottom-only
  - rotating cube now shows directional-light variation instead of flat unlit shading

## Risks

- Mesh normal orientation may not match the shader space on the first pass.
- The generated model path may expand indexed geometry in a way that needs normal duplication logic.
- The citro3d shader uniform layout may require iteration to keep matrix and light data aligned.
