# Helengine Nintendo 3DS Host

This repository contains the Nintendo 3DS platform host and builder integration for Helengine.

## Build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform 3ds `
  -Output ..\helprojs\city\3ds-build
```

## Run In Emulator

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 `
  -ArtifactPath ..\helprojs\city\3ds-build\helengine_3ds.3dsx
```

## More Docs

- [Docker Build Notes](docs/Docker.md)
- [Platform Notes](docs/PlatformNotes.md)
