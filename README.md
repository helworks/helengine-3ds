# Helengine Nintendo 3DS Host

This repository contains the native Nintendo 3DS host scaffold for Helengine.

## Current milestone

- Docker-only build using devkitPro Nintendo 3DS tooling
- Native `.3dsx` output for direct loading in Citra or Lime3DS
- First boot check with a green top screen and cyan bottom screen
- Real citro2d and citro3d renderer startup for later 3D rendering work

## Build

```bash
docker build -t helengine-3ds .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-3ds make
```

The build emits `build/helengine_3ds.3dsx`.

## Generated core seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot check

Load `build/helengine_3ds.3dsx` in Citra or Lime3DS. The expected result for this milestone is a green top screen and a cyan bottom screen with no immediate crash or reset loop.
