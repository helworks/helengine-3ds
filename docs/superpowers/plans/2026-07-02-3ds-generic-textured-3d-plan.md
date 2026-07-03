# 3DS Generic Textured 3D Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a generic Nintendo 3DS 3D textured-material path for the current standard opaque diffuse-texture contract so textured scenes render with their cooked diffuse textures instead of plain white base color.

**Architecture:** Keep the current 3DS lit untextured mesh path intact and add a second lit textured branch selected per submesh material. Extend the runtime model to preserve UVs, extend the runtime material to retain and own diffuse textures, and reuse the existing 3DS runtime texture upload path for 3D material binding.

**Tech Stack:** C++, Citro3D, Citro2D runtime texture uploader, C# xUnit source tests, PowerShell build/launch scripts, city generated scene assets.

---

## File Map

### Existing files to modify

- `C:/dev/helworks/helengine/engine/helengine.editor.tests/CityCubeTestSceneSourceTests.cs`
  - Extend source-level coverage for the textured-scene contract or add a focused 3DS runtime source audit.
- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsModelVertex.hpp`
  - Expand the 3DS interleaved vertex contract from `position + normal` to `position + uv + normal`.
- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeModel.hpp`
  - Keep the runtime model aligned with the expanded vertex format.
- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeModel.cpp`
  - Keep runtime-model construction and disposal aligned with the expanded vertex format.
- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeMaterial.hpp`
  - Add diffuse texture path and owned runtime texture state.
- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeMaterial.cpp`
  - Implement the new runtime-material accessors and ownership defaults.
- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.hpp`
  - Add textured-branch shader handles, uniform locations, and helper declarations.
- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.cpp`
  - Preserve UVs, load cooked diffuse textures, select textured vs untextured shader branches, and bind `GPU_TEXTURE0`.

### New files to create

- `C:/dev/helworks/helengine-3ds/src/platform/3ds/lit_textured.v.pica`
  - 3DS vertex shader for lit textured meshes using `position + uv + normal`.

### Existing files to verify but not modify unless required

- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeTexture.hpp`
- `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeTexture.cpp`
- `C:/dev/helworks/helengine-3ds/builder/Nintendo3DsPlatformAssetBuilder.cs`
- `C:/dev/helprojs/city/assets/codebase/rendering.tools/TexturedCubeGridSceneFactory.cs`

---

### Task 1: Lock The Textured 3DS Runtime Contract In Tests

**Files:**
- Modify: `C:/dev/helworks/helengine/engine/helengine.editor.tests/CityCubeTestSceneSourceTests.cs`
- Test: `C:/dev/helworks/helengine/engine/helengine.editor.tests/helengine.editor.tests.csproj`

- [ ] **Step 1: Write the failing source test**

Add a new test to assert the 3DS renderer source is no longer base-color-only and has an explicit textured branch seam.

```csharp
/// <summary>
/// Ensures the Nintendo 3DS renderer preserves the generic textured-material branch needed by textured cube scenes.
/// </summary>
[Fact]
public void Nintendo3ds_render_manager_source_supports_textured_3d_material_branch() {
    string sourcePath = @"C:\dev\helworks\helengine-3ds\src\platform\3ds\Nintendo3DsRenderManager3D.cpp";
    string source = File.ReadAllText(sourcePath);

    Assert.Contains("TextureRelativePath", source, StringComparison.Ordinal);
    Assert.Contains("Nintendo3DsRuntimeTexture", source, StringComparison.Ordinal);
    Assert.Contains("C3D_TexBind(0,", source, StringComparison.Ordinal);
    Assert.Contains("lit_textured_shbin", source, StringComparison.Ordinal);
}
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run:

```powershell
dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter Nintendo3ds_render_manager_source_supports_textured_3d_material_branch
```

Expected:

- `FAIL`
- the failure should report that `TextureRelativePath`, `Nintendo3DsRuntimeTexture`, `C3D_TexBind(0,`, or `lit_textured_shbin` is missing from `Nintendo3DsRenderManager3D.cpp`

- [ ] **Step 3: Commit the red test**

```powershell
git -C C:\dev\helworks\helengine add -- engine/helengine.editor.tests/CityCubeTestSceneSourceTests.cs
git -C C:\dev\helworks\helengine commit -m "test: lock 3ds textured 3d renderer seam"
```

---

### Task 2: Preserve UVs In The 3DS Runtime Vertex Stream

**Files:**
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsModelVertex.hpp`
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeModel.hpp`
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeModel.cpp`
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.cpp`

- [ ] **Step 1: Expand the runtime vertex struct**

Update `Nintendo3DsModelVertex` to carry UVs between position and normal.

```cpp
struct Nintendo3DsModelVertex final {
    float3 Position;
    float2 TextureCoordinate;
    float3 Normal;
};
```

- [ ] **Step 2: Update the 3DS runtime model interfaces to keep the expanded vertex type**

Keep constructor and getter signatures based on `Nintendo3DsModelVertex*`.

```cpp
Nintendo3DsRuntimeModel(Nintendo3DsModelVertex* vertexData, int32_t vertexCount);
Nintendo3DsModelVertex* GetVertexData() const;
Nintendo3DsModelVertex* VertexData;
```

- [ ] **Step 3: Expand model building to preserve UVs**

In `Nintendo3DsRenderManager3D::BuildModelFromRaw`, copy UVs alongside positions and normals.

```cpp
if (data->TextureCoordinates == nullptr || data->TextureCoordinates->Length != data->Positions->Length) {
    vertex.TextureCoordinate = float2(0.0f, 0.0f);
} else {
    vertex.TextureCoordinate = (*data->TextureCoordinates)[sourceIndex];
}
```

Use the same logic in both indexed and non-indexed expansion branches.

- [ ] **Step 4: Run the focused editor test again**

Run:

```powershell
dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter Nintendo3ds_render_manager_source_supports_textured_3d_material_branch
```

Expected:

- still `FAIL`
- but now only on texture-material/shader binding expectations, not on UV preservation if you extended the test further

- [ ] **Step 5: Commit the UV-preservation slice**

```powershell
git -C C:\dev\helworks\helengine-3ds add -- src/platform/3ds/Nintendo3DsModelVertex.hpp src/platform/3ds/Nintendo3DsRuntimeModel.hpp src/platform/3ds/Nintendo3DsRuntimeModel.cpp src/platform/3ds/Nintendo3DsRenderManager3D.cpp
git -C C:\dev\helworks\helengine-3ds commit -m "feat: preserve uv data in 3ds runtime meshes"
```

---

### Task 3: Add Diffuse Texture State To 3DS Runtime Materials

**Files:**
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeMaterial.hpp`
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeMaterial.cpp`

- [ ] **Step 1: Add runtime-material fields and accessors**

Extend the runtime material with a cooked texture path and an owned 3DS runtime texture.

```cpp
const std::string& GetTextureRelativePath() const;
void SetTextureRelativePath(std::string value);

Nintendo3DsRuntimeTexture* GetOwnedDiffuseTexture() const;
void SetOwnedDiffuseTexture(Nintendo3DsRuntimeTexture* value);
```

Private fields:

```cpp
std::string TextureRelativePathValue;
Nintendo3DsRuntimeTexture* OwnedDiffuseTextureValue;
```

- [ ] **Step 2: Implement safe ownership defaults**

Initialize the new fields in the constructor and release the owned texture in the destructor or disposal path used by the material.

```cpp
Nintendo3DsRuntimeMaterial::Nintendo3DsRuntimeMaterial()
    : RuntimeMaterial()
    , BaseColorValue(1.0f, 1.0f, 1.0f, 1.0f)
    , TextureRelativePathValue()
    , OwnedDiffuseTextureValue(nullptr) {
}
```

If you add a destructor:

```cpp
Nintendo3DsRuntimeMaterial::~Nintendo3DsRuntimeMaterial() {
    if (OwnedDiffuseTextureValue != nullptr) {
        delete OwnedDiffuseTextureValue;
        OwnedDiffuseTextureValue = nullptr;
    }
}
```

- [ ] **Step 3: Run a compile-oriented 3DS build check**

Run:

```powershell
$env:HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID='cube_test_ds'
powershell -NoProfile -ExecutionPolicy Bypass -Command "& 'C:\dev\helworks\helengine\artifacts\build-platform.ps1' -Project 'C:\dev\helprojs\city\project.heproj' -Platform 3ds -Output 'C:\dev\helprojs\city\3ds-build'"
```

Expected:

- build may still fail later on missing shader/binding code
- it should not fail on `Nintendo3DsRuntimeMaterial` type/member declarations

- [ ] **Step 4: Commit the runtime-material state slice**

```powershell
git -C C:\dev\helworks\helengine-3ds add -- src/platform/3ds/Nintendo3DsRuntimeMaterial.hpp src/platform/3ds/Nintendo3DsRuntimeMaterial.cpp
git -C C:\dev\helworks\helengine-3ds commit -m "feat: retain diffuse texture state in 3ds runtime materials"
```

---

### Task 4: Load Cooked Diffuse Textures For 3DS 3D Materials

**Files:**
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.hpp`
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.cpp`
- Verify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeTexture.hpp`
- Verify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRuntimeTexture.cpp`

- [ ] **Step 1: Preserve cooked texture path in `BuildMaterialFromCooked(PlatformMaterialAsset*)`**

After setting base color and lighting mode, preserve the cooked texture path:

```cpp
runtimeMaterial->SetTextureRelativePath(materialAsset->TextureRelativePath);
```

- [ ] **Step 2: Add a helper that loads a cooked texture asset when a material has a path**

Add a helper declaration in the header:

```cpp
void AttachCookedDiffuseTexture(Nintendo3DsRuntimeMaterial* runtimeMaterial, const std::string& cookedMaterialAssetPath);
```

Implement it in the `.cpp` file by:

1. deserializing the cooked material asset path’s sibling texture asset path
2. loading a `TextureAsset`
3. creating a `Nintendo3DsRuntimeTexture`
4. calling `LoadFromRaw`
5. storing it on the runtime material

Use the same deserialize pattern already used by other 3DS cooked-asset readers:

```cpp
FileStream* stream = File::OpenRead(cookedTextureAssetPath);
Asset* asset = AssetSerializer::Deserialize(stream);
TextureAsset* cookedTextureAsset = he_cpp_try_cast<TextureAsset>(asset);
Nintendo3DsRuntimeTexture* runtimeTexture = new Nintendo3DsRuntimeTexture();
runtimeTexture->LoadFromRaw(cookedTextureAsset);
runtimeMaterial->SetOwnedDiffuseTexture(runtimeTexture);
```

- [ ] **Step 3: Invoke the helper from `BuildMaterialFromCooked(std::string cookedAssetPath)`**

After building the runtime material from the deserialized `PlatformMaterialAsset`, call the helper when the runtime material has a non-empty texture path.

```cpp
Nintendo3DsRuntimeMaterial* runtimeMaterial = static_cast<Nintendo3DsRuntimeMaterial*>(BuildMaterialFromCooked(cookedMaterialAsset));
if (!runtimeMaterial->GetTextureRelativePath().empty()) {
    AttachCookedDiffuseTexture(runtimeMaterial, cookedAssetPath);
}
```

- [ ] **Step 4: Run the focused editor test**

Run:

```powershell
dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter Nintendo3ds_render_manager_source_supports_textured_3d_material_branch
```

Expected:

- still `FAIL`
- remaining failures should be about actual shader/binding strings such as `C3D_TexBind(0,` or `lit_textured_shbin`

- [ ] **Step 5: Commit the texture-loading slice**

```powershell
git -C C:\dev\helworks\helengine-3ds add -- src/platform/3ds/Nintendo3DsRenderManager3D.hpp src/platform/3ds/Nintendo3DsRenderManager3D.cpp
git -C C:\dev\helworks\helengine-3ds commit -m "feat: load cooked diffuse textures for 3ds 3d materials"
```

---

### Task 5: Add The Lit Textured 3DS Shader

**Files:**
- Create: `C:/dev/helworks/helengine-3ds/src/platform/3ds/lit_textured.v.pica`
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.hpp`
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.cpp`

- [ ] **Step 1: Add the textured shader source**

Create a shader that takes:

- `v0 = position`
- `v1 = texcoord`
- `v2 = normal`

and outputs:

- transformed position
- texture coordinates
- lit primary color

Base skeleton:

```asm
.fvec projection[4]
.fvec modelView[4]
.fvec lightVec
.fvec lightClr
.fvec ambientClr
.fvec baseColor

.constf constants(0.0, 1.0, -1.0, 0.0)
.alias zeros constants.xxxx
.alias ones constants.yyyy

.out outpos position
.out outtc0 texcoord0
.out outclr color

.alias inpos v0
.alias intex v1
.alias innrm v2
```

The body should mirror the current Lambert math from `lit_color.v.pica`, plus:

```asm
mov outtc0, intex
```

- [ ] **Step 2: Add textured-program fields to the 3DS renderer**

Add:

```cpp
DVLB_s* TexturedVertexShaderDvlb;
shaderProgram_s TexturedProgram;
bool HasTexturedShaderProgram;
int TexturedUniformLocationProjection;
int TexturedUniformLocationModelView;
int TexturedUniformLocationLightVector;
int TexturedUniformLocationLightColor;
int TexturedUniformLocationAmbientColor;
int TexturedUniformLocationBaseColor;
```

- [ ] **Step 3: Initialize and release the textured program**

Add `EnsureTexturedShaderInitialized()` and extend `ReleaseShaderResources()`.

Initialization skeleton:

```cpp
TexturedVertexShaderDvlb = DVLB_ParseFile((u32*)lit_textured_shbin, lit_textured_shbin_size);
shaderProgramInit(&TexturedProgram);
shaderProgramSetVsh(&TexturedProgram, &TexturedVertexShaderDvlb->DVLE[0]);
TexturedUniformLocationProjection = shaderInstanceGetUniformLocation(TexturedProgram.vertexShader, "projection");
```

- [ ] **Step 4: Run the focused editor test**

Run:

```powershell
dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter Nintendo3ds_render_manager_source_supports_textured_3d_material_branch
```

Expected:

- may still `FAIL`
- the remaining missing string should only be `C3D_TexBind(0,` if the draw path is not wired yet

- [ ] **Step 5: Commit the shader-program slice**

```powershell
git -C C:\dev\helworks\helengine-3ds add -- src/platform/3ds/lit_textured.v.pica src/platform/3ds/Nintendo3DsRenderManager3D.hpp src/platform/3ds/Nintendo3DsRenderManager3D.cpp
git -C C:\dev\helworks\helengine-3ds commit -m "feat: add lit textured shader for 3ds meshes"
```

---

### Task 6: Branch Draw Submission Between Untextured And Textured Materials

**Files:**
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.hpp`
- Modify: `C:/dev/helworks/helengine-3ds/src/platform/3ds/Nintendo3DsRenderManager3D.cpp`

- [ ] **Step 1: Add branch-local pipeline helpers**

Declare helpers such as:

```cpp
void ApplyUntexturedPipelineState();
void ApplyTexturedPipelineState();
void ApplyCommonLightingUniforms(
    shaderProgram_s& program,
    int projectionLocation,
    int modelViewLocation,
    int lightVectorLocation,
    int lightColorLocation,
    int ambientColorLocation,
    int baseColorLocation,
    const C3D_Mtx& projectionMatrix,
    const C3D_Mtx& modelViewMatrix,
    const float3& viewSpaceLightVector,
    const float4& directionalLightColor,
    const float4& ambientLightColor,
    const float4& baseColor);
```

- [ ] **Step 2: Configure the textured pipeline**

Textured attr/buffer layout should match `position + uv + normal`:

```cpp
AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);
AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3);
BufInfo_Add(bufInfo, vertexData, sizeof(Nintendo3DsModelVertex), 3, 0x210);
```

Textured texenv should use the sampled texture modulated by vertex primary color:

```cpp
C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
```

- [ ] **Step 3: Bind the texture and select the shader per submesh**

Inside `DrawRuntimeModel`, branch on the resolved runtime material:

```cpp
Nintendo3DsRuntimeTexture* diffuseTexture = runtimeMaterial == nullptr
    ? nullptr
    : runtimeMaterial->GetOwnedDiffuseTexture();

if (diffuseTexture != nullptr && diffuseTexture->HasNativeTexture()) {
    EnsureTexturedShaderInitialized();
    C3D_BindProgram(&TexturedProgram);
    ApplyTexturedPipelineState();
    C3D_TexBind(0, diffuseTexture->GetNativeTexture());
} else {
    EnsureShaderInitialized();
    C3D_BindProgram(&Program);
    ApplyUntexturedPipelineState();
}
```

- [ ] **Step 4: Keep untextured scenes working**

Do not remove the current untextured branch. `cube_test_ds` should continue to render through the existing lit-color path when no diffuse texture is present.

- [ ] **Step 5: Run the focused editor test to green**

Run:

```powershell
dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter Nintendo3ds_render_manager_source_supports_textured_3d_material_branch
```

Expected:

- `PASS`

- [ ] **Step 6: Commit the draw-branch slice**

```powershell
git -C C:\dev\helworks\helengine-3ds add -- src/platform/3ds/Nintendo3DsRenderManager3D.hpp src/platform/3ds/Nintendo3DsRenderManager3D.cpp
git -C C:\dev\helworks\helengine-3ds commit -m "feat: branch 3ds mesh draws between textured and untextured materials"
```

---

### Task 7: Verify The Generic Textured Path End-To-End

**Files:**
- Verify: `C:/dev/helprojs/city/assets/codebase/rendering.tools/TexturedCubeGridSceneFactory.cs`
- Verify build output: `C:/dev/helprojs/city/3ds-build/helengine_3ds.3dsx`

- [ ] **Step 1: Build the 3DS artifact for the textured cube scene**

Run:

```powershell
$env:HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID='textured_cube_grid_ds'
powershell -NoProfile -ExecutionPolicy Bypass -Command "& 'C:\dev\helworks\helengine\artifacts\build-platform.ps1' -Project 'C:\dev\helprojs\city\project.heproj' -Platform 3ds -Output 'C:\dev\helprojs\city\3ds-build'"
```

Expected:

- `Build completed for platform '3ds': C:\dev\helprojs\city\3ds-build`

- [ ] **Step 2: Launch Azahar into `textured_cube_grid_ds`**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-3ds\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx
```

Expected:

- `ARTIFACT=C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx`
- `PROCESS_ID=<number>`
- textured cubes should no longer render as flat white cubes

- [ ] **Step 3: Rebuild into `cube_test_ds` for the untextured smoke check**

Run:

```powershell
$env:HELENGINE_GENERATED_BOOT_SCENE_INITIAL_SCENE_ID='cube_test_ds'
powershell -NoProfile -ExecutionPolicy Bypass -Command "& 'C:\dev\helworks\helengine\artifacts\build-platform.ps1' -Project 'C:\dev\helprojs\city\project.heproj' -Platform 3ds -Output 'C:\dev\helprojs\city\3ds-build'"
```

Expected:

- `Build completed for platform '3ds': C:\dev\helprojs\city\3ds-build`

- [ ] **Step 4: Relaunch Azahar into `cube_test_ds`**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine-3ds\scripts\launch_in_emulator.ps1 -ArtifactPath C:\dev\helprojs\city\3ds-build\helengine_3ds.3dsx
```

Expected:

- `PROCESS_ID=<number>`
- `cube_test_ds` should still render correctly with lit but untextured geometry

- [ ] **Step 5: Commit the verified feature**

```powershell
git -C C:\dev\helworks\helengine-3ds add -- src/platform/3ds docs/superpowers/specs docs/superpowers/plans
git -C C:\dev\helworks\helengine add -- engine/helengine.editor.tests/CityCubeTestSceneSourceTests.cs
git -C C:\dev\helworks\helengine-3ds commit -m "feat: add generic textured 3d materials for 3ds"
```

---

## Self-Review

### Spec coverage

- runtime UV preservation: covered by Task 2
- runtime material texture state: covered by Task 3
- cooked diffuse texture loading: covered by Task 4
- textured shader branch: covered by Task 5
- per-submesh textured/untextured draw selection: covered by Task 6
- textured-scene and untextured-scene verification: covered by Task 7

### Placeholder scan

- no `TODO`/`TBD` placeholders remain
- each task includes exact file paths
- each test/build step includes an exact command and expected result

### Type consistency

- `Nintendo3DsModelVertex` carries `Position`, `TextureCoordinate`, and `Normal`
- `Nintendo3DsRuntimeMaterial` carries `GetTextureRelativePath()` and `GetOwnedDiffuseTexture()`
- `lit_textured_shbin` is the generated header implied by `lit_textured.v.pica`

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-02-3ds-generic-textured-3d-plan.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
