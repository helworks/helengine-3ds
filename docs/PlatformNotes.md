# Nintendo 3DS Platform Notes

## Current Milestone

- Docker-only build using devkitPro Nintendo 3DS tooling
- Managed 3DS builder scaffold under `builder/`
- Native `.3dsx` output for direct loading in Citra, Lime3DS, or Azahar
- Builder-owned startup manifest packaged through RomFS
- Generated-core compile and startup on native 3DS
- Startup scene loading through the runtime scene catalog and `SceneManager`
- Real citro2d and citro3d renderer startup for later 3D rendering work
- Dual-screen 2D viewport routing for DS-authored scene layouts

## Builder Tests

```bash
dotnet test builder.tests/helengine.3ds.builder.tests.csproj
```

## Generated Core Seam

The native build accepts generated-core output through `HELENGINE_CORE_CPP_ROOT`.

When provided, the 3DS player compiles generated C++ runtime code, loads startup scenes through the generated runtime scene catalog, applies standard platform input bindings from the generated runtime manifest, and renders 2D scene content across top and bottom screens through authored camera viewports.

## RomFS Startup Manifest Check

The 3DS builder writes `runtime/3ds_startup_manifest.bin` into a staged RomFS root and the runtime loads it from `romfs:/runtime/3ds_startup_manifest.bin`.

To test the native package manually without the editor, create a staged RomFS root with a manifest and pass it into the Docker build:

```bash
mkdir -p /tmp/helengine-3ds-manual/runtime
printf '\x48\x33\x53\x50\x01\x00\x08\x00\xFF\x00\x00\xFF\x00\x00\xFF\xFF' > /tmp/helengine-3ds-manual/runtime/3ds_startup_manifest.bin
docker run --rm -v "$PWD":/workspace -v /tmp/helengine-3ds-manual:/workspace-staging -w /workspace helengine-3ds make HELENGINE_3DS_ROMFS_ROOT=/workspace-staging
```

Load `build/helengine_3ds.3dsx` in Citra or Lime3DS. The expected result is a red top screen and a blue bottom screen. If the manifest is missing or invalid, the runtime remains on the green/cyan bootstrap frame.

## Audio In Emulator

Nintendo 3DS homebrew audio runs through libctru NDSP. Direct `.3dsx` launches in Azahar, Citra, or Lime3DS need the DSP component to be available through one of these paths:

- `/3ds/dspfirm.cdc` on the emulated SD card
- `hb:ndsp` supplied by the Homebrew Launcher environment

If NDSP cannot initialize, the runtime keeps rendering but audio will stay silent and the host writes `sdmc:/helengine_3ds_last_error.txt` with `phase=audio-init` plus the failing result code.

## DS Scene Contract Verification

The current 3DS runtime follows the Nintendo DS startup-scene contract at the platform seam:

- effective startup scene: `GeneratedBootScene`
- expected DS menu target: `DemoDiscMainMenuDs`
- startup scene transitions resolved through the runtime scene catalog
- top and bottom screen layout driven by the authored viewport system
