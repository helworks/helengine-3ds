# 3DS Generic Textured 3D Design

## Summary

Add a generic Nintendo 3DS 3D textured-material path for the current standard opaque diffuse-texture contract.

The 3DS builder already cooks `TextureRelativePath` into `PlatformMaterialAsset`, but the 3DS 3D runtime currently drops texture data and only renders base-color-lit meshes. The result is that textured 3D scenes such as `textured_cube_grid_ds` render as white cubes.

This change extends the existing 3DS lit 3D path so it can:

- preserve authored UVs in runtime models
- preserve cooked diffuse texture references in runtime materials
- load cooked diffuse textures for 3D materials
- bind a diffuse texture for textured submeshes
- render textured and untextured materials side by side in the same scene

## Problem

Current 3DS 3D rendering has four gaps:

1. `Nintendo3DsRenderManager3D` expands only positions and normals from `ModelAsset` and ignores UVs.
2. `Nintendo3DsRuntimeMaterial` stores only base color and lighting mode.
3. `BuildMaterialFromCooked` preserves base color but does not load or retain the cooked diffuse texture path.
4. The current 3DS mesh shader and pipeline never bind `GPU_TEXTURE0` or sample a texture.

Because of those gaps, any standard material that depends on a diffuse texture falls back to plain base color on 3DS.

## Goal

Support the current standard opaque diffuse-texture material contract generically on 3DS 3D meshes.

“Generically” here means:

- the path is not special-cased to `textured_cube_grid_ds`
- textured and untextured materials can coexist in the same 3DS scene
- scene authoring remains unchanged
- the builder-owned material payload remains the source of truth

## Non-Goals

This pass does not add:

- vertex-color modulation for 3D materials
- normal mapping
- emissive maps
- alpha blending or cutout materials
- material-schema expansion beyond the current standard opaque diffuse-texture contract
- post-build asset rewriting or generated-code patching

## Design

### 1. Runtime Vertex Format

Extend the 3DS runtime mesh vertex format from:

- `position + normal`

to:

- `position + uv + normal`

`Nintendo3DsRenderManager3D::BuildModelFromRaw` should expand UVs alongside positions and normals when the source model provides them.

Rules:

- if a model has no UV data, store zero UVs and allow the material path to remain untextured
- normals remain required for the current lit path
- the runtime vertex buffer stays linear-allocated and interleaved for Citro3D submission

This keeps the renderer generic across textured and untextured meshes.

### 2. Runtime Material Contract

Extend `Nintendo3DsRuntimeMaterial` so it carries:

- normalized base color
- lighting model
- cooked diffuse texture relative path
- optional loaded `Nintendo3DsRuntimeTexture`

Rules:

- untextured materials are valid and keep using base color
- textured materials remain valid even if the diffuse texture path is absent, but they will render through the untextured branch
- runtime ownership of the loaded diffuse texture belongs to the runtime material

This mirrors the existing pattern already used by other platform renderers without making the 3DS renderer depend on scene-specific code.

### 3. Cooked Texture Loading

When `Nintendo3DsRenderManager3D` builds a runtime material from a cooked material path:

1. deserialize `PlatformMaterialAsset`
2. build the base runtime material
3. if `TextureRelativePath` is not empty, resolve and load the cooked texture asset into a `Nintendo3DsRuntimeTexture`
4. store both the relative path and the loaded runtime texture on the material

This should reuse the existing 3DS runtime texture upload path rather than inventing a second texture format or upload system.

### 4. Shader and Pipeline Branches

Keep two 3DS 3D shader branches:

- existing lit untextured branch
- new lit textured branch

The new lit textured branch should:

- accept `position + uv + normal`
- bind `GPU_TEXTURE0`
- sample the diffuse texture
- modulate sampled color by base color
- apply the same directional-light Lambert response as the current lit path

Renderer behavior:

- if a material has a loaded diffuse texture, use the textured branch
- otherwise use the untextured branch

This preserves the current 3DS lit behavior for scenes such as `cube_test_ds` while enabling textured scenes.

### 5. Draw-Time Material Selection

Per submesh draw:

1. resolve the runtime material for that submesh slot
2. detect whether the material has a valid native diffuse texture
3. select the correct shader, attr layout, buffer layout, and texenv state
4. bind the diffuse texture when present
5. submit the draw

This branch stays local to the 3DS renderer and does not require scene or generated-core changes.

## Files Expected To Change

### 3DS Runtime

- `src/platform/3ds/Nintendo3DsModelVertex.hpp`
- `src/platform/3ds/Nintendo3DsRuntimeModel.hpp`
- `src/platform/3ds/Nintendo3DsRuntimeModel.cpp`
- `src/platform/3ds/Nintendo3DsRuntimeMaterial.hpp`
- `src/platform/3ds/Nintendo3DsRuntimeMaterial.cpp`
- `src/platform/3ds/Nintendo3DsRenderManager3D.hpp`
- `src/platform/3ds/Nintendo3DsRenderManager3D.cpp`
- new textured 3DS shader source under `src/platform/3ds/`

### Tests

- source-level 3DS/city coverage proving the textured scene authoring contract and/or runtime contract assumptions needed by this path

## Verification

Minimum verification for this change:

1. add or update a focused failing source/runtime test first
2. make the test pass
3. rebuild 3DS with `HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID=textured_cube_grid_ds`
4. launch Azahar
5. visually confirm that textured cubes are no longer white and still preserve authored scene structure
6. smoke-check that `cube_test_ds` still renders correctly after the textured branch is introduced

## Risks

### UV layout mismatch

If the Citro3D attribute layout does not match the interleaved runtime vertex layout, textures will appear scrambled or static.

Mitigation:

- keep the vertex struct explicit
- keep attr loader order aligned with shader input order
- use one branch-local `BufInfo_Add` stride definition per shader path

### Texture upload works in 2D but not in 3D

The existing 3DS runtime texture path is already used by 2D, but 3D still needs explicit `C3D_TexBind` and fragment-stage configuration.

Mitigation:

- reuse `Nintendo3DsRuntimeTexture`
- add the smallest possible texture-binding branch in 3D
- validate with one known textured scene

### Regression in untextured lit scenes

Switching the 3DS runtime vertex layout affects all 3D meshes.

Mitigation:

- keep the untextured branch intact
- keep `cube_test_ds` as a regression smoke test

## Recommendation

Implement the generic two-branch renderer:

- untextured lit branch for current non-textured materials
- textured lit branch for current standard opaque diffuse-texture materials

This is the smallest design that is still correct, generic, and compatible with the current 3DS runtime architecture.
