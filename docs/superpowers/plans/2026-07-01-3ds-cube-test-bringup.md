# 3DS Cube Test Bring-Up Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `cube_test` render correctly on the Nintendo 3DS top screen through the canonical build and launch flow.

**Architecture:** Reproduce the current 3DS build and launch path, compare the 3DS generated-runtime and matrix seam against the working GameCube and Wii implementations, then implement the smallest shared and 3DS-local fixes needed for correct top-screen rendering. Keep the bottom screen minimal during bring-up and avoid generated-code patching.

**Tech Stack:** C#, .NET 9, HelEngine editor/build pipeline, generated C++ runtime, Nintendo 3DS host runtime, citro2d/citro3d, PowerShell build and launch scripts.

---

### Task 1: Reproduce The Current 3DS Path

**Files:**
- Read: `README.md`
- Read: `docs/PlatformNotes.md`
- Read: `builder/Nintendo3DsPlatformDefinitionFactory.cs`

- [ ] **Step 1: Run the canonical 3DS build**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform 3ds -Output C:\dev\helprojs\city\3ds-build`

- [ ] **Step 2: Confirm the artifact exists and capture its timestamp**

Run: `Get-Item C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx`

- [ ] **Step 3: Launch the artifact through the repo script**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx`

- [ ] **Step 4: Record whether failure is build-time, startup-time, or render-time**

Expected: a concrete failure point with logs or visible runtime behavior.

### Task 2: Compare The Matrix And Runtime Seams

**Files:**
- Read: `builder/Nintendo3DsPlatformDefinitionFactory.cs`
- Read: `C:\dev\helworks\helengine-gc\builder\GameCubePlatformDefinitionFactory.cs`
- Read: `C:\dev\helworks\helengine-wii\builder\WiiPlatformDefinitionFactory.cs`
- Read: relevant 3DS native renderer files under `src/platform/3ds`

- [ ] **Step 1: Verify the 3DS builder requests the intended runtime generation contract**

Expected: identify whether 3DS already requests the same generated-runtime bootstrap and platform-adapted matrix contract pattern as GC/Wii.

- [ ] **Step 2: Compare the 3DS native matrix upload path against the working GC/Wii implementation**

Expected: determine whether the fault is shared codegen output or a 3DS-local matrix conversion/upload issue.

### Task 3: Implement The Smallest Correct Fix

**Files:**
- Modify: shared HelEngine generation/runtime files only if the defect is generic
- Modify: 3DS host builder/runtime files only where 3DS-specific adaptation is required

- [ ] **Step 1: Apply the shared seam fix if the comparison shows a generic defect**

Expected: 3DS consumes the correct generated runtime contract without generated-file rewrites.

- [ ] **Step 2: Apply the minimal 3DS-local renderer fix if citro3d upload or projection conversion still differs**

Expected: top-screen `cube_test` uses the correct transform path.

- [ ] **Step 3: Keep diagnostics temporary and remove them once the root cause is verified**

Expected: no permanent noisy probe logging unless it has lasting diagnostic value.

### Task 4: Validate The Bring-Up

**Files:**
- Verify: `C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx`

- [ ] **Step 1: Re-run the canonical 3DS build**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 -Project C:\dev\helprojs\city\project.heproj -Platform 3ds -Output C:\dev\helprojs\city\3ds-build`

- [ ] **Step 2: Launch the rebuilt artifact**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx`

- [ ] **Step 3: Confirm `cube_test` renders correctly on the top screen**

Expected: the cube scene renders correctly, even if bottom-screen parity is deferred.
