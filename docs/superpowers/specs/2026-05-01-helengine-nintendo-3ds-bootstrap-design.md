# Helengine Nintendo 3DS Bootstrap Design

## Goal

Create the first Nintendo 3DS native bootstrap for Helengine as a Docker-only build that produces a runnable `.3dsx` artifact. The first emulator validation target is a stable top green screen and bottom cyan screen in Citra or Lime3DS, with stereoscopic 3D disabled for this milestone.

## Scope

This milestone covers only the native Nintendo 3DS host bootstrap and its build environment.

Included:
- Docker-based devkitARM 3DS build environment
- Native build script that emits `build/helengine_3ds.3dsx`
- Thin native entrypoint
- Nintendo 3DS boot host that initializes the real 3DS rendering stack and holds the verification colors
- Minimal README with build and emulator verification instructions
- Reserved `HELENGINE_CORE_CPP_ROOT` seam for future generated-core integration

Excluded:
- Generated core compilation
- Engine/game logic integration
- Asset pipeline
- Input handling
- Audio
- Stereoscopic 3D rendering behavior
- Scene rendering beyond the color-clear verification frame

## Repository Context

`helengine-3ds` is being created from scratch for this work. The structure should remain intentionally parallel to the GameCube and Nintendo DS bootstrap repositories so platform host repositories stay predictable.

## Architecture

The project should remain minimal and platform-focused:

- `Dockerfile`
  Builds a 3DS development container from a devkitPro devkitARM image and installs the 3DS development package set required for libctru, citro3d, and citro2d.
- `Makefile`
  Builds the Nintendo 3DS target, emits `build/helengine_3ds.3dsx`, and reserves `HELENGINE_CORE_CPP_ROOT` for later generated-core integration.
- `README.md`
  Documents the Docker build flow and emulator verification target.
- `src/main.cpp`
  Owns only construction and execution of the 3DS host bootstrap.
- `src/platform/3ds/Nintendo3DsBootHost.hpp`
  Declares the thin Nintendo 3DS bootstrap boundary.
- `src/platform/3ds/Nintendo3DsBootHost.cpp`
  Owns libctru startup, citro3d/citro2d initialization, render-target setup for top and bottom screens, and the infinite frame loop that clears top to green and bottom to cyan.

The first milestone deliberately initializes the actual 3DS rendering stack instead of using raw framebuffer writes. That keeps the visual target simple while ensuring later 3D rendering work can grow from the same host bootstrap instead of replacing it.

## Build Environment Design

The Docker image should use a devkitPro devkitARM base image rather than manually installing toolchains from a host package manager. The image must explicitly set:

- `DEVKITPRO=/opt/devkitpro`
- `DEVKITARM=${DEVKITPRO}/devkitARM`
- `PORTLIBS=${DEVKITPRO}/portlibs/3ds`
- `PATH` entries for devkitARM binaries and devkitPro tooling

The Dockerfile should not use a `# syntax=docker/dockerfile:...` directive unless the file genuinely requires it. The GameCube work already showed that unnecessary frontend resolution can create avoidable Docker auth failures on the host machine.

The image must install the Nintendo 3DS development package group used by modern devkitPro tooling rather than assuming the base image already contains the full 3DS graphics stack.

## Build Script Design

The Makefile should follow the same principles used in the GameCube and Nintendo DS repositories after the toolchain debugging work:

- avoid relying on implicit PATH lookup for core tools when a stable absolute path is available
- prefer the current devkitPro 3DS platform rules over stale hardcoded assumptions
- keep all intermediate files under `build/`
- reserve `HELENGINE_CORE_CPP_ROOT` exactly as a future seam, not as an active dependency

The Makefile should emit:

- `build/helengine_3ds.elf`
- `build/helengine_3ds.3dsx`

It should link against the 3DS graphics stack needed for the milestone:

- `libctru`
- `citro3d`
- `citro2d`

If the current devkitARM image provides updated 3DS rules or package layout details, the implementation should adapt to the actual toolchain layout rather than forcing an outdated template.

## Runtime Behavior Design

The 3DS bootstrap should initialize both visible screens into a deterministic visual state suitable for emulator verification.

Target behavior:

- top display: solid green
- bottom display: solid cyan
- stereoscopic 3D path: disabled or unused
- application remains alive indefinitely
- no crash loop
- no automatic exit

The runtime should use the actual rendering lifecycle intended for later engine work:

1. initialize libctru
2. initialize citro3d
3. initialize citro2d
4. create top and bottom render targets
5. begin a frame each loop
6. clear the top target to green
7. clear the bottom target to cyan
8. end the frame and present

The milestone should not add text consoles, input systems, or extra rendering abstractions unless the 3DS API requires them for correct setup.

## Data and Control Flow

The runtime flow should remain straightforward:

1. `main()` constructs `Nintendo3DsBootHost`
2. `main()` calls `Run()`
3. `Run()` initializes `gfxInitDefault`
4. `Run()` initializes `C3D_Init`
5. `Run()` initializes `C2D_Init`
6. `Run()` creates top and bottom render targets
7. `Run()` enters an infinite frame loop that clears top to green and bottom to cyan

No other subsystems are part of the first milestone.

## Error Handling

This milestone should not introduce silent fallbacks or synthetic defaults. The build should fail if:

- the toolchain path assumptions are wrong
- the 3DS development package set is missing
- the graphics libraries are absent
- the linker rules differ from the Makefile assumptions

The implementation should correct those root causes in the Dockerfile or Makefile rather than masking them in runtime code.

## Verification Plan

Repository-level verification:

```bash
docker build -t helengine-3ds .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-3ds make
```

Expected build result:

- `build/helengine_3ds.3dsx` exists

Emulator-level verification:

- open `build/helengine_3ds.3dsx` in Citra or Lime3DS
- confirm the top screen is solid green
- confirm the bottom screen is solid cyan
- confirm the process remains stable without resetting or exiting immediately
- confirm no stereoscopic 3D behavior is required for the milestone

## Testing Strategy

There is no meaningful automated runtime emulator test in this first milestone. Verification is therefore split between:

- build verification in Docker
- manual boot verification in a 3DS emulator

The implementation should still preserve a small, testable structure by keeping the native entrypoint thin and centralizing 3DS platform startup inside a dedicated host class.

## Constraints

- Docker-only build flow
- Native Nintendo 3DS output only
- `.3dsx` artifact only for this milestone
- No generated core integration yet
- Minimal architecture, intentionally parallel to the GameCube and Nintendo DS bootstraps
- Use the real 3DS rendering stack now so later 3D rendering grows from the same host bootstrap

## Recommended Implementation Direction

Use a minimal devkitARM plus 3DS Docker scaffold, a Makefile aligned to the current devkitPro 3DS toolchain layout, and a single `Nintendo3DsBootHost` that owns `gfxInitDefault`, citro3d/citro2d startup, render-target creation, and the verification frame loop. This keeps the first milestone simple while making the platform bootstrap genuinely ready for later 3D rendering work.
