# 3DS Lit Cube Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the 3DS `cube_test_ds` cube respond to the authored directional light while preserving the fixed top-screen clear color and mixed 2D plus 3D presentation.

**Architecture:** Extend the existing 3DS runtime-model path to preserve normals, replace the current solid-color shader path with a minimal directional-light Lambert path, and resolve one active directional light from the captured scene for the top-screen 3D pass. Keep the frame pipeline and DS/3DS overlay routing unchanged.

**Tech Stack:** C++, citro3d, citro2d, generated-core runtime model data, PICA shader binary assets, PowerShell build scripts

---

### Task 1: Lock in the expected scene behavior

**Files:**
- Modify: `C:\dev\helworks\helengine\engine\helengine.editor.tests\CityCubeTestSceneSourceTests.cs`

- [ ] **Step 1: Add or extend the scene-source assertion for the lit cube contract**
- [ ] **Step 2: Run the focused test and verify it fails only if the expected scene contract is missing**
- [ ] **Step 3: Keep the source test green with the existing cube-test authored light/material setup**

### Task 2: Preserve normals in the 3DS runtime model path

**Files:**
- Modify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRuntimeModel.hpp`
- Modify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRuntimeModel.cpp`
- Modify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRenderManager3D.cpp`

- [ ] **Step 1: Inspect the current expanded vertex path and define the position-plus-normal runtime vertex layout**
- [ ] **Step 2: Update the runtime model storage to own the expanded lit vertex stream**
- [ ] **Step 3: Update model build logic to expand normals alongside positions and fail fast if lit rendering requires missing normals**
- [ ] **Step 4: Update the 3DS buffer submission path to use the new vertex layout**

### Task 3: Replace the solid-color shader path with a minimal lit shader

**Files:**
- Modify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRenderManager3D.hpp`
- Modify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRenderManager3D.cpp`
- Modify or add: `C:\dev\helworks\helengine-3ds\src\platform\3ds\*.pica`
- Modify or add: `C:\dev\helworks\helengine-3ds\src\platform\3ds\*_shbin.h`

- [ ] **Step 1: Identify how the current shader binary is produced and where the lit shader source should live**
- [ ] **Step 2: Add a minimal lit shader that consumes position, normal, projection, model-view, light direction, and light color**
- [ ] **Step 3: Update renderer shader initialization, attribute layout, and uniform binding for the lit path**
- [ ] **Step 4: Keep the fixed-function pipeline state minimal and compatible with the mixed 2D plus 3D frame flow**

### Task 4: Resolve one authored directional light for the 3DS 3D pass

**Files:**
- Modify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRenderManager3D.hpp`
- Modify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRenderManager3D.cpp`

- [ ] **Step 1: Inspect how scene lights are exposed through generated-core objects available to the renderer**
- [ ] **Step 2: Capture the first enabled directional light relevant to the top-screen scene**
- [ ] **Step 3: Convert the authored light orientation/color into the shader uniform data used by the lit pass**
- [ ] **Step 4: Keep a safe fallback when no directional light exists**

### Task 5: Verify the integrated 3DS result

**Files:**
- Verify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsBootHost.cpp`
- Verify: `C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRenderManager3D.cpp`
- Verify: `C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx`

- [ ] **Step 1: Rebuild 3DS with `HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID=cube_test_ds`**
- [ ] **Step 2: Launch the fresh artifact in Azahar**
- [ ] **Step 3: Confirm the top screen keeps the authored cornflower-blue clear and top-only `Hello World`**
- [ ] **Step 4: Confirm the rotating cube now shows directional-light variation instead of flat unlit shading**
