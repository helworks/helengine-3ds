using helengine.baseplatform.Definitions;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.baseplatform.Targets;
using helengine.files;
using helengine.nintendo3ds.builder.tests.Builders;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.Versioning;

namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Verifies the Nintendo 3DS builder metadata exposed to the editor.
/// </summary>
[SupportedOSPlatform("windows")]
public class Nintendo3DsPlatformAssetBuilderTests {
    /// <summary>
    /// Verifies the builder descriptor and platform definition expose the expected Nintendo 3DS metadata.
    /// </summary>
    [Fact]
    public void Descriptor_and_definition_expose_3ds_metadata() {
        Nintendo3DsPlatformAssetBuilder builder = new();

        Assert.Equal("helengine.3ds.builder", builder.Descriptor.BuilderId);
        Assert.Equal("1.0.0", builder.Descriptor.BuilderVersion);
        Assert.Equal("3ds", builder.Descriptor.TargetPlatformId);
        Assert.Equal("3ds", builder.Definition.PlatformId);
        Assert.Contains(builder.Definition.BuildProfiles, profile => profile.ProfileId == "3ds-default");
        Assert.Contains(builder.Definition.GraphicsProfiles, profile => profile.ProfileId == "3ds-bootstrap");
        Assert.Contains(builder.Definition.StorageProfiles, profile =>
            profile.ProfileId == "romfs-package" &&
            profile.RuntimeSpecializationId == "3ds-romfs-package");
    }

    /// <summary>
    /// Verifies the Nintendo 3DS platform definition publishes one explicit generic texture cook contract for sprites and font atlases.
    /// </summary>
    [Fact]
    public void Definition_exposes_3ds_texture_cook_capabilities() {
        Nintendo3DsPlatformAssetBuilder builder = new();

        PlatformAssetCookCapabilityDefinition textureCapability = Assert.Single(
            builder.Definition.AssetCookCapabilities,
            capability => capability.SourceAssetKind == "texture");
        Assert.Equal("runtime-texture", textureCapability.TargetArtifactKind);
        Assert.Equal(PlatformAssetCookOwnershipKind.BuilderOwned, textureCapability.OwnershipKind);
        Assert.Equal("3ds-texture", textureCapability.SettingsContractId);
        Assert.NotNull(textureCapability.TextureFormatCapabilities);
        Assert.Contains(TextureAssetColorFormat.Rgba32.ToString(), textureCapability.TextureFormatCapabilities.SupportedColorFormatIds);
        Assert.Contains(TextureAssetAlphaPrecision.A8, textureCapability.TextureFormatCapabilities.SupportedAlphaPrecisions);

        PlatformAssetCookCapabilityDefinition fontAtlasCapability = Assert.Single(
            builder.Definition.AssetCookCapabilities,
            capability => capability.SourceAssetKind == "font-atlas-texture");
        Assert.Equal("runtime-texture", fontAtlasCapability.TargetArtifactKind);
        Assert.Equal(PlatformAssetCookOwnershipKind.BuilderOwned, fontAtlasCapability.OwnershipKind);
        Assert.Equal("3ds-font-atlas-texture", fontAtlasCapability.SettingsContractId);
        Assert.NotNull(fontAtlasCapability.TextureFormatCapabilities);
        Assert.Contains(TextureAssetColorFormat.Rgba32.ToString(), fontAtlasCapability.TextureFormatCapabilities.SupportedColorFormatIds);
        Assert.Contains(TextureAssetAlphaPrecision.A8, fontAtlasCapability.TextureFormatCapabilities.SupportedAlphaPrecisions);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS platform definition publishes one fixed-pipeline material schema for platform-owned runtime materials.
    /// </summary>
    [Fact]
    public void Definition_exposes_3ds_material_schema() {
        Nintendo3DsPlatformAssetBuilder builder = new();

        PlatformMaterialSchemaDefinition materialSchema = Assert.Single(builder.Definition.MaterialSchemas);
        Assert.Equal("3ds-standard-textured", materialSchema.SchemaId);
        Assert.Contains("3ds-bootstrap", materialSchema.GraphicsProfileIds);
        Assert.Contains(materialSchema.Fields, field => field.FieldId == "texture-relative-path");
        Assert.Contains(materialSchema.Fields, field => field.FieldId == "double-sided");
        Assert.Contains(materialSchema.Fields, field => field.FieldId == "vertex-color-mode");
        Assert.Contains(materialSchema.Fields, field => field.FieldId == "base-color");
        Assert.Contains(materialSchema.Fields, field => field.FieldId == "lighting-mode");
    }

    /// <summary>
    /// Verifies the Nintendo 3DS builder cooks one standard textured material into the generic platform-owned runtime payload.
    /// </summary>
    [Fact]
    public void CookMaterial_whenUsingStandardTexturedSchema_returnsPlatformMaterialAsset() {
        Nintendo3DsPlatformAssetBuilder builder = new();
        Dictionary<string, string> fieldValues = new(StringComparer.Ordinal) {
            ["texture-relative-path"] = "cooked/imported/menu-logo.hasset",
            ["double-sided"] = "true",
            ["vertex-color-mode"] = "multiply",
            ["base-color"] = "#804020FF",
            ["lighting-mode"] = "unlit"
        };

        PlatformMaterialCookResult result = builder.CookMaterial(new PlatformMaterialCookRequest(
            "materials/menu.material",
            "assets/materials/menu.material",
            "3ds",
            "3ds-default",
            "3ds-bootstrap",
            "3ds-standard-textured",
            fieldValues));

        PlatformMaterialAsset cookedAsset = Assert.IsType<PlatformMaterialAsset>(helengine.files.AssetSerializer.DeserializeFromBytes(result.CookedMaterialBytes));
        Assert.Equal("materials/menu.material", cookedAsset.Id);
        Assert.Equal("3ds-bootstrap", cookedAsset.RendererFamilyId);
        Assert.Equal("cooked/imported/menu-logo.hasset", cookedAsset.TextureRelativePath);
        Assert.True(cookedAsset.DoubleSided);
        Assert.True(cookedAsset.UseVertexColor);
        Assert.False(cookedAsset.Lit);
        Assert.Equal((byte)0x80, cookedAsset.BaseColorR);
        Assert.Equal((byte)0x40, cookedAsset.BaseColorG);
        Assert.Equal((byte)0x20, cookedAsset.BaseColorB);
        Assert.Equal((byte)0xFF, cookedAsset.BaseColorA);
        Assert.Empty(result.ReferencedShaderAssetIds);
    }

    /// <summary>
    /// Verifies the platform plugin manifest points to the Nintendo 3DS builder assembly and definition factory.
    /// </summary>
    [Fact]
    public void Platform_plugin_manifest_exposes_expected_builder_metadata() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string pluginManifestPath = Path.Combine(repositoryRootPath, "platform-plugin.json");
        string pluginManifestSource = File.ReadAllText(pluginManifestPath);

        Assert.Contains("\"platformId\": \"3ds\"", pluginManifestSource, StringComparison.Ordinal);
        Assert.Contains("\"builderAssemblyPath\": \"builder/helengine.3ds.builder.dll\"", pluginManifestSource, StringComparison.Ordinal);
        Assert.Contains("\"definitionFactoryType\": \"helengine.nintendo3ds.builder.Nintendo3DsPlatformDefinitionFactory\"", pluginManifestSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the default repository-root resolution points at the Nintendo 3DS repository instead of the builder project directory.
    /// </summary>
    [Fact]
    public void ResolveRepositoryRootPath_whenNoEnvironmentOverrideIsPresent_returns_repository_root() {
        string previousRepositoryRootOverride = Environment.GetEnvironmentVariable("HELENGINE_3DS_REPOSITORY_ROOT");
        string expectedRepositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));

        try {
            Environment.SetEnvironmentVariable("HELENGINE_3DS_REPOSITORY_ROOT", null);

            string resolvedRepositoryRootPath = Nintendo3DsPlatformAssetBuilder.ResolveRepositoryRootPath();

            Assert.Equal(expectedRepositoryRootPath, resolvedRepositoryRootPath);
        } finally {
            Environment.SetEnvironmentVariable("HELENGINE_3DS_REPOSITORY_ROOT", previousRepositoryRootOverride);
        }
    }

    /// <summary>
    /// Verifies repository-root resolution walks upward until it finds the Nintendo 3DS repository markers.
    /// </summary>
    [Fact]
    public void ResolveRepositoryRootPath_whenAssemblyDirectoryIsNested_beyond_bin_returns_repository_root() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string simulatedAssemblyDirectoryPath = Path.Combine(repositoryRootPath, "builder", "obj", "Debug", "net9.0");

        string resolvedRepositoryRootPath = Nintendo3DsPlatformAssetBuilder.ResolveRepositoryRootPath(simulatedAssemblyDirectoryPath);

        Assert.Equal(repositoryRootPath, resolvedRepositoryRootPath);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS native build script accepts a staged RomFS root and embeds it in the final package.
    /// </summary>
    [Fact]
    public void Makefile_whenPackagingRomFs_accepts_staged_romfs_root() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefilePath = Path.Combine(repositoryRootPath, "Makefile");
        string makefileSource = File.ReadAllText(makefilePath);

        Assert.Contains("HELENGINE_3DS_ROMFS_ROOT", makefileSource, StringComparison.Ordinal);
        Assert.Contains("--romfs=", makefileSource, StringComparison.Ordinal);
        Assert.Contains("export _3DSXFLAGS", makefileSource, StringComparison.Ordinal);
        Assert.Contains("export _3DSXDEPS", makefileSource, StringComparison.Ordinal);
        Assert.Contains("--smdh=", makefileSource, StringComparison.Ordinal);
        Assert.Contains("$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)", makefileSource.Replace("\t", string.Empty), StringComparison.Ordinal);
        Assert.DoesNotContain("export _3DSXFLAGS +=", makefileSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS native build script validates and compiles generated core sources when a generated C++ root is supplied.
    /// </summary>
    [Fact]
    public void Makefile_whenGeneratedCoreIsEnabled_validates_and_compiles_generated_sources() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefilePath = Path.Combine(repositoryRootPath, "Makefile");
        string makefileSource = File.ReadAllText(makefilePath);

        Assert.Contains("helcpp_config.hpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("helengine_core_amalgamated.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("helengine_core_unity.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("runtime/runtime_startup_manifest.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("GENERATED_CORE_SOURCE_DIRS := $(HELENGINE_CORE_CPP_ROOT) $(HELENGINE_CORE_CPP_ROOT)/runtime", makefileSource, StringComparison.Ordinal);
        Assert.Contains("GENERATED_CORE_CPPFILES := $(GENERATED_CORE_TRANSLATION_UNIT) runtime_startup_manifest.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("runtime_scene_catalog_manifest.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("runtime_code_module_manifest.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("runtime_standard_platform_input_manifest.cpp", makefileSource, StringComparison.Ordinal);
        Assert.Contains("ifneq ($(GENERATED_CORE_TRANSLATION_UNIT),helengine_core_unity.cpp)", makefileSource, StringComparison.Ordinal);
        Assert.Contains("CXXFLAGS := $(CFLAGS) -std=gnu++20", makefileSource, StringComparison.Ordinal);
        Assert.Contains("ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)", makefileSource, StringComparison.Ordinal);
        Assert.Contains("CXXFLAGS += -fno-rtti", makefileSource, StringComparison.Ordinal);
        Assert.Contains("CXXFLAGS += -fno-exceptions", makefileSource, StringComparison.Ordinal);
        Assert.Contains("export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(GENERATED_CORE_SOURCE_DIRS)", makefileSource, StringComparison.Ordinal);
        Assert.Contains("CPPFILES += $(GENERATED_CORE_CPPFILES)", makefileSource, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the build flow writes the startup manifest into the staged RomFS root and delegates native packaging through the executor seam.
    /// </summary>
    [Fact]
    public async Task BuildAsync_writes_startup_manifest_and_invokes_native_executor() {
        string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-build-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string builderWorkingRoot = Path.Combine(workingRoot, "tmp");
        string packageRoot = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(builderWorkingRoot);
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string startupSceneId = "mainmenu";
        string startupSceneRelativePath = "cooked/scenes/mainmenu.hasset";

        try {
            Directory.CreateDirectory(Path.Combine(packageRoot, "cooked", "scenes"));
            Directory.CreateDirectory(generatedCoreRoot);
            File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "// test");
            Directory.CreateDirectory(outputRoot);
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", "mainmenu.hasset"),
                [0x47, 0x42, 0x53]);

            RecordingDiagnosticReporter diagnosticReporter = new();
            RecordingProgressReporter progressReporter = new();
            FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
            Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, repositoryRoot);

            PlatformBuildScene[] scenes = [
                new PlatformBuildScene(
                    startupSceneId,
                    "Main Menu",
                    "scene",
                    [new PlatformBuildPayloadReference(
                        startupSceneRelativePath,
                        startupSceneRelativePath)],
                    [new KeyValuePair<string, string>(
                        PlatformBuildSceneMetadataKeys.CookedRelativePath,
                        startupSceneRelativePath)])
            ];

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "3ds",
                "1",
                startupSceneId,
                scenes,
                Array.Empty<PlatformBuildAsset>(),
                Array.Empty<PlatformBuildArtifact>(),
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("3ds-romfs-package", Array.Empty<PlatformContainerArtifact>()));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("3ds-default", "3ds", "3ds", "3ds-default")],
                [new PlatformCookProfile(
                    "3ds-default",
                    "3DS Default",
                    new PlatformCookProfileCapabilities(
                        "3ds",
                        "raw",
                        "raw",
                        "3ds-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                builderWorkingRoot,
                selectedBuildProfileId: "3ds-default",
                selectedGraphicsProfileId: "3ds-bootstrap",
                selectedCodegenProfileId: "default",
                selectedBuildOptionValues: new Dictionary<string, string> {
                    ["startup-top-screen-color"] = "#FF0000",
                    ["startup-bottom-screen-color"] = "#0000FF"
                },
                selectedGraphicsOptionValues: new Dictionary<string, string>(),
                selectedCodegenOptionValues: new Dictionary<string, string>(),
                generatedCoreCppRootPath: generatedCoreRoot,
                selectedMediaProfileId: "3dsx-homebrew",
                selectedStorageProfileId: "romfs-package");

            var report = await builder.BuildAsync(
                request,
                progressReporter,
                diagnosticReporter,
                CancellationToken.None);

            Assert.True(report.Succeeded);
            Assert.NotNull(nativeBuildExecutor.Workspace);
            Assert.Equal(Path.GetFullPath(generatedCoreRoot), nativeBuildExecutor.Workspace.GeneratedCoreRootPath);
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "runtime", "3ds_startup_manifest.bin")));
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "mainmenu.hasset")));
            Assert.Contains(
                progressReporter.Updates,
                update => update.StageName == "Stage Packaged Content" &&
                    update.CurrentItemIdentity == startupSceneRelativePath);
            Assert.True(File.Exists(nativeBuildExecutor.Workspace.ExportPackagePath));
            Assert.Empty(diagnosticReporter.Diagnostics);
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, recursive: true);
            }
        }
    }

    /// <summary>
    /// Verifies builder-owned 3DS texture cook work items are executed into the package source root before RomFS staging.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenTextureCookWorkItemIsPresent_cooksAndStagesTexturePayload() {
        string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-build-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string builderWorkingRoot = Path.Combine(workingRoot, "tmp");
        string packageRoot = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(builderWorkingRoot);
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string sourceTexturePath = Path.Combine(workingRoot, "menu-logo.png");
        string startupSceneId = "mainmenu";
        string startupSceneRelativePath = "cooked/scenes/mainmenu.hasset";

        try {
            Directory.CreateDirectory(Path.Combine(packageRoot, "cooked", "scenes"));
            Directory.CreateDirectory(generatedCoreRoot);
            File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "// test");
            Directory.CreateDirectory(outputRoot);
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", "mainmenu.hasset"),
                [0x47, 0x42, 0x53]);
            using (Bitmap bitmap = new(2, 1)) {
                bitmap.SetPixel(0, 0, Color.FromArgb(255, 255, 0, 0));
                bitmap.SetPixel(1, 0, Color.FromArgb(255, 0, 0, 255));
                bitmap.Save(sourceTexturePath, ImageFormat.Png);
            }

            RecordingDiagnosticReporter diagnosticReporter = new();
            RecordingProgressReporter progressReporter = new();
            FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
            Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, repositoryRoot);

            PlatformBuildScene[] scenes = [
                new PlatformBuildScene(
                    startupSceneId,
                    "Main Menu",
                    "scene",
                    [new PlatformBuildPayloadReference(
                        startupSceneRelativePath,
                        startupSceneRelativePath)],
                    [new KeyValuePair<string, string>(
                        PlatformBuildSceneMetadataKeys.CookedRelativePath,
                        startupSceneRelativePath)])
            ];

            PlatformCookWorkItem[] platformCookWorkItems = [
                new PlatformCookWorkItem(
                    "menu-logo-work-item",
                    sourceTexturePath,
                    "texture",
                    "3ds",
                    "runtime-texture",
                    "cooked/imported/menu-logo.hasset",
                    "menu-logo",
                    "source-hash",
                    "settings-hash",
                    Nintendo3DsTextureCookSettingsSerializer.Serialize(1024, TextureAssetColorFormat.Rgba32, TextureAssetAlphaPrecision.A8),
                    [new PlatformCookWorkItemMetadata("source-asset-id", "menu-logo")])
            ];

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "3ds",
                "1",
                startupSceneId,
                scenes,
                Array.Empty<PlatformBuildAsset>(),
                Array.Empty<PlatformBuildArtifact>(),
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("3ds-romfs-package", Array.Empty<PlatformContainerArtifact>()),
                platformCookWorkItems);

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("3ds-default", "3ds", "3ds", "3ds-default")],
                [new PlatformCookProfile(
                    "3ds-default",
                    "3DS Default",
                    new PlatformCookProfileCapabilities(
                        "3ds",
                        "raw",
                        "raw",
                        "3ds-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                builderWorkingRoot,
                selectedBuildProfileId: "3ds-default",
                selectedGraphicsProfileId: "3ds-bootstrap",
                selectedCodegenProfileId: "default",
                selectedBuildOptionValues: new Dictionary<string, string> {
                    ["startup-top-screen-color"] = "#FF0000",
                    ["startup-bottom-screen-color"] = "#0000FF"
                },
                selectedGraphicsOptionValues: new Dictionary<string, string>(),
                selectedCodegenOptionValues: new Dictionary<string, string>(),
                generatedCoreCppRootPath: generatedCoreRoot,
                selectedMediaProfileId: "3dsx-homebrew",
                selectedStorageProfileId: "romfs-package");

            await builder.BuildAsync(
                request,
                progressReporter,
                diagnosticReporter,
                CancellationToken.None);

            string stagedTexturePath = Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "imported", "menu-logo.hasset");
            Assert.True(File.Exists(stagedTexturePath));
            TextureAsset cookedTexture = Assert.IsType<TextureAsset>(helengine.files.AssetSerializer.DeserializeFromBytes(File.ReadAllBytes(stagedTexturePath)));
            Assert.Equal("menu-logo", cookedTexture.Id);
            Assert.Equal((ushort)2, cookedTexture.Width);
            Assert.Equal((ushort)1, cookedTexture.Height);
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, recursive: true);
            }
        }
    }

    /// <summary>
    /// Verifies builder-owned cooked outputs are normalized into canonical lowercase packaged paths before RomFS staging begins.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenCookWorkItemOutputPathUsesUppercase_normalizesPathAndBuildsSuccessfully() {
        string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-build-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string builderWorkingRoot = Path.Combine(workingRoot, "tmp");
        string packageRoot = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(builderWorkingRoot);
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string sourceTexturePath = Path.Combine(workingRoot, "menu-logo.png");
        string startupSceneId = "mainmenu";
        string startupSceneRelativePath = "cooked/scenes/mainmenu.hasset";

        try {
            Directory.CreateDirectory(Path.Combine(packageRoot, "cooked", "scenes"));
            Directory.CreateDirectory(generatedCoreRoot);
            File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "// test");
            Directory.CreateDirectory(outputRoot);
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", "mainmenu.hasset"),
                [0x47, 0x42, 0x53]);
            using (Bitmap bitmap = new(1, 1)) {
                bitmap.SetPixel(0, 0, Color.FromArgb(255, 255, 255, 255));
                bitmap.Save(sourceTexturePath, ImageFormat.Png);
            }

            FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
            RecordingDiagnosticReporter diagnosticReporter = new();
            RecordingProgressReporter progressReporter = new();
            Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, repositoryRoot);
            PlatformBuildScene[] scenes = [
                new PlatformBuildScene(
                    startupSceneId,
                    "Main Menu",
                    "scene",
                    [new PlatformBuildPayloadReference(
                        startupSceneRelativePath,
                        startupSceneRelativePath)],
                    [new KeyValuePair<string, string>(
                        PlatformBuildSceneMetadataKeys.CookedRelativePath,
                        startupSceneRelativePath)])
            ];
            PlatformCookWorkItem[] platformCookWorkItems = [
                new PlatformCookWorkItem(
                    "menu-logo-work-item",
                    sourceTexturePath,
                    "texture",
                    "3ds",
                    "runtime-texture",
                    "cooked/Imported/menu-logo.hasset",
                    "menu-logo",
                    "source-hash",
                    "settings-hash",
                    Nintendo3DsTextureCookSettingsSerializer.Serialize(1024, TextureAssetColorFormat.Rgba32, TextureAssetAlphaPrecision.A8),
                    [new PlatformCookWorkItemMetadata("source-asset-id", "menu-logo")])
            ];
            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "3ds",
                "1",
                startupSceneId,
                scenes,
                Array.Empty<PlatformBuildAsset>(),
                Array.Empty<PlatformBuildArtifact>(),
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("3ds-romfs-package", Array.Empty<PlatformContainerArtifact>()),
                platformCookWorkItems);
            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("3ds-default", "3ds", "3ds", "3ds-default")],
                [new PlatformCookProfile(
                    "3ds-default",
                    "3DS Default",
                    new PlatformCookProfileCapabilities(
                        "3ds",
                        "raw",
                        "raw",
                        "3ds-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                builderWorkingRoot,
                selectedBuildProfileId: "3ds-default",
                selectedGraphicsProfileId: "3ds-bootstrap",
                selectedCodegenProfileId: "default",
                selectedBuildOptionValues: new Dictionary<string, string> {
                    ["startup-top-screen-color"] = "#FF0000",
                    ["startup-bottom-screen-color"] = "#0000FF"
                },
                selectedGraphicsOptionValues: new Dictionary<string, string>(),
                selectedCodegenOptionValues: new Dictionary<string, string>(),
                generatedCoreCppRootPath: generatedCoreRoot,
                selectedMediaProfileId: "3dsx-homebrew",
                selectedStorageProfileId: "romfs-package");

            await builder.BuildAsync(
                request,
                progressReporter,
                diagnosticReporter,
                CancellationToken.None);

            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "mainmenu.hasset")));
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, recursive: true);
            }
        }
    }

    /// <summary>
    /// Verifies Nintendo 3DS honors the manifest startup-scene id even when the authored startup scene is not the first scene in the manifest.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenManifestStartupSceneIsNotFirstScene_reportsConfiguredStartupScenePath() {
        string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-build-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string builderWorkingRoot = Path.Combine(workingRoot, "tmp");
        string packageRoot = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(builderWorkingRoot);
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string introSceneRelativePath = "cooked/scenes/intro.hasset";
        string startupSceneId = "mainmenu";
        string startupSceneRelativePath = "cooked/scenes/mainmenu.hasset";

        try {
            Directory.CreateDirectory(Path.Combine(packageRoot, "cooked", "scenes"));
            Directory.CreateDirectory(generatedCoreRoot);
            Directory.CreateDirectory(outputRoot);
            File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "// test");
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", "intro.hasset"),
                [0x11, 0x22, 0x33]);
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", "mainmenu.hasset"),
                [0x47, 0x42, 0x53]);

            FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
            RecordingDiagnosticReporter diagnosticReporter = new();
            RecordingProgressReporter progressReporter = new();
            Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, repositoryRoot);
            PlatformBuildScene[] scenes = [
                new PlatformBuildScene(
                    "Intro",
                    "Intro",
                    "scene",
                    [new PlatformBuildPayloadReference(introSceneRelativePath, introSceneRelativePath)],
                    [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, introSceneRelativePath)]),
                new PlatformBuildScene(
                    startupSceneId,
                    "Main Menu",
                    "scene",
                    [new PlatformBuildPayloadReference(startupSceneRelativePath, startupSceneRelativePath)],
                    [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, startupSceneRelativePath)])
            ];

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "3ds",
                "1",
                startupSceneId,
                scenes,
                Array.Empty<PlatformBuildAsset>(),
                Array.Empty<PlatformBuildArtifact>(),
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("3ds-romfs-package", Array.Empty<PlatformContainerArtifact>()));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("3ds-default", "3ds", "3ds", "3ds-default")],
                [new PlatformCookProfile(
                    "3ds-default",
                    "3DS Default",
                    new PlatformCookProfileCapabilities(
                        "3ds",
                        "raw",
                        "raw",
                        "3ds-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                builderWorkingRoot,
                selectedBuildProfileId: "3ds-default",
                selectedGraphicsProfileId: "3ds-bootstrap",
                selectedCodegenProfileId: "default",
                selectedBuildOptionValues: new Dictionary<string, string> {
                    ["startup-top-screen-color"] = "#FF0000",
                    ["startup-bottom-screen-color"] = "#0000FF"
                },
                selectedGraphicsOptionValues: new Dictionary<string, string>(),
                selectedCodegenOptionValues: new Dictionary<string, string>(),
                generatedCoreCppRootPath: generatedCoreRoot,
                selectedMediaProfileId: "3dsx-homebrew",
                selectedStorageProfileId: "romfs-package");

            await builder.BuildAsync(
                request,
                progressReporter,
                diagnosticReporter,
                CancellationToken.None);

            Assert.Contains(
                progressReporter.Updates,
                update => update.StageName == "Stage Packaged Content" &&
                    update.CurrentItemIdentity == startupSceneRelativePath);
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, recursive: true);
            }
        }
    }

    /// <summary>
    /// Verifies Nintendo 3DS stages authored startup scenes at their declared cooked-relative path without creating a platform-owned alias.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenStartupSceneUsesGeneratedBuildPath_stagesDeclaredScenePayloadPath() {
        string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-build-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string builderWorkingRoot = Path.Combine(workingRoot, "tmp");
        string packageRoot = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(builderWorkingRoot);
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");
        string startupSceneId = "mainmenu";
        string startupSceneRelativePath = "cooked/scenes/.generated-build/3ds/test-build/mainmenu_test-build.hasset";

        try {
            Directory.CreateDirectory(Path.Combine(packageRoot, "cooked", "scenes", ".generated-build", "3ds", "test-build"));
            Directory.CreateDirectory(generatedCoreRoot);
            Directory.CreateDirectory(outputRoot);
            File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "// test");
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", ".generated-build", "3ds", "test-build", "mainmenu_test-build.hasset"),
                [0x47, 0x42, 0x53]);

            FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
            RecordingDiagnosticReporter diagnosticReporter = new();
            RecordingProgressReporter progressReporter = new();
            Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, repositoryRoot);
            PlatformBuildScene[] scenes = [
                new PlatformBuildScene(
                    startupSceneId,
                    "Main Menu",
                    "scene",
                    [new PlatformBuildPayloadReference(startupSceneRelativePath, startupSceneRelativePath)],
                    [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, startupSceneRelativePath)])
            ];

            PlatformBuildManifest manifest = new(
                1,
                "project",
                "1.0.0",
                "1.0.0",
                "3ds",
                "1",
                startupSceneId,
                scenes,
                Array.Empty<PlatformBuildAsset>(),
                Array.Empty<PlatformBuildArtifact>(),
                Array.Empty<PlatformBuildCodeModule>(),
                Array.Empty<PlatformArtifactPlacement>(),
                new PlatformContainerWritePlan("3ds-romfs-package", Array.Empty<PlatformContainerArtifact>()));

            PlatformBuildRequest request = new(
                manifest,
                [new PlatformBuildTargetVariant("3ds-default", "3ds", "3ds", "3ds-default")],
                [new PlatformCookProfile(
                    "3ds-default",
                    "3DS Default",
                    new PlatformCookProfileCapabilities(
                        "3ds",
                        "raw",
                        "raw",
                        "3ds-scene-v1",
                        PlatformSerializationEndianness.LittleEndian))],
                outputRoot,
                builderWorkingRoot,
                selectedBuildProfileId: "3ds-default",
                selectedGraphicsProfileId: "3ds-bootstrap",
                selectedCodegenProfileId: "default",
                selectedBuildOptionValues: new Dictionary<string, string> {
                    ["startup-top-screen-color"] = "#FF0000",
                    ["startup-bottom-screen-color"] = "#0000FF"
                },
                selectedGraphicsOptionValues: new Dictionary<string, string>(),
                selectedCodegenOptionValues: new Dictionary<string, string>(),
                generatedCoreCppRootPath: generatedCoreRoot,
                selectedMediaProfileId: "3dsx-homebrew",
                selectedStorageProfileId: "romfs-package");

            await builder.BuildAsync(
                request,
                progressReporter,
                diagnosticReporter,
                CancellationToken.None);

            Assert.Equal(
                [0x47, 0x42, 0x53],
                File.ReadAllBytes(
                    Path.Combine(
                        nativeBuildExecutor.Workspace.RomFsRootPath,
                        "cooked",
                        "scenes",
                        ".generated-build",
                        "3ds",
                        "test-build",
                        "mainmenu_test-build.hasset")));
            Assert.False(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "generatedbootscene.hasset")));
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, recursive: true);
            }
        }
    }
}
