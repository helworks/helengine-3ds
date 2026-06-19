# Nintendo 3DS Docker Build

Use the local Docker image when you need to build the Nintendo 3DS host directly.

```bash
docker build -t helengine-3ds .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-3ds make
```

The build emits `build/helengine_3ds.3dsx`.
