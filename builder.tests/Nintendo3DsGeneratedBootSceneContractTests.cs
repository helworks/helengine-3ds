using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.nintendo3ds.builder.tests.Builders;
using System.Runtime.Versioning;

namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Verifies the Nintendo 3DS builder adopts the current direct main-menu startup contract.
/// </summary>
[SupportedOSPlatform("windows")]
public class Nintendo3DsGeneratedBootSceneContractTests {
    /// <summary>
    /// Verifies the 3DS platform exposes the direct startup scene id and cooked relative path explicitly.
    /// </summary>
    [Fact]
    public void StartupSceneIds_expose_direct_startup_scene_contract() {
        Assert.Equal("DemoDiscMainMenu", Nintendo3DsStartupSceneIds.DirectStartupSceneId);
        Assert.Equal("cooked/scenes/DemoDiscMainMenu.hasset", Nintendo3DsStartupSceneIds.DirectStartupSceneCookedRelativePath);
    }

    /// <summary>
    /// Verifies Nintendo 3DS builds use the direct console main menu as the effective startup scene even when the authored manifest names another startup scene.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenManifestStartupSceneDiffersFromBootSceneContract_usesDirectMainMenuAsEffectiveStartupScene() {
        string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-build-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string builderWorkingRoot = Path.Combine(workingRoot, "tmp");
        string packageRoot = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(builderWorkingRoot);
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");

        try {
            Directory.CreateDirectory(Path.Combine(packageRoot, "cooked", "scenes"));
            Directory.CreateDirectory(generatedCoreRoot);
            Directory.CreateDirectory(outputRoot);
            File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "// test");
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", "DemoDiscMainMenu.hasset"),
                [0x44, 0x4D, 0x4D]);
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", "GeneratedBootScene.hasset"),
                [0x47, 0x42, 0x53]);

            RecordingDiagnosticReporter diagnosticReporter = new();
            RecordingProgressReporter progressReporter = new();
            FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
            Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, repositoryRoot);

            PlatformBuildScene[] scenes = [
                new PlatformBuildScene(
                    "GeneratedBootScene",
                    "Generated Boot Scene",
                    "scene",
                    [new PlatformBuildPayloadReference("cooked/scenes/GeneratedBootScene.hasset", "cooked/scenes/GeneratedBootScene.hasset")],
                    [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, "cooked/scenes/GeneratedBootScene.hasset")]),
                new PlatformBuildScene(
                    Nintendo3DsStartupSceneIds.DirectStartupSceneId,
                    "Demo Disc Main Menu",
                    "scene",
                    [new PlatformBuildPayloadReference(
                        Nintendo3DsStartupSceneIds.DirectStartupSceneCookedRelativePath,
                        Nintendo3DsStartupSceneIds.DirectStartupSceneCookedRelativePath)],
                    [new KeyValuePair<string, string>(
                        PlatformBuildSceneMetadataKeys.CookedRelativePath,
                        Nintendo3DsStartupSceneIds.DirectStartupSceneCookedRelativePath)])
            ];

            PlatformBuildManifest manifest = new(
                2,
                "project",
                "1.0.0",
                "1.0.0",
                "3ds",
                "1",
                "GeneratedBootScene",
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

            Assert.NotNull(nativeBuildExecutor.Workspace);
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "DemoDiscMainMenu.hasset")));
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "GeneratedBootScene.hasset")));
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, true);
            }
        }
    }

    /// <summary>
    /// Verifies Nintendo 3DS builds stage every payload referenced by packaged scenes, including generated engine material assets.
    /// </summary>
    [Fact]
    public async Task BuildAsync_whenScenePayloadReferencesGeneratedEngineMaterial_stagesGeneratedMaterialIntoRomFs() {
        string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-build-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string builderWorkingRoot = Path.Combine(workingRoot, "tmp");
        string packageRoot = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(builderWorkingRoot);
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");

        try {
            Directory.CreateDirectory(Path.Combine(packageRoot, "cooked", "scenes"));
            Directory.CreateDirectory(Path.Combine(packageRoot, "cooked", "engine", "materials"));
            Directory.CreateDirectory(generatedCoreRoot);
            Directory.CreateDirectory(outputRoot);
            File.WriteAllText(Path.Combine(generatedCoreRoot, "helcpp_config.hpp"), "// test");
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "scenes", "DemoDiscMainMenu.hasset"),
                [0x44, 0x4D, 0x4D]);
            File.WriteAllBytes(
                Path.Combine(packageRoot, "cooked", "engine", "materials", "standard.hasset"),
                [0x53, 0x54, 0x44]);

            RecordingDiagnosticReporter diagnosticReporter = new();
            RecordingProgressReporter progressReporter = new();
            FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
            Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, repositoryRoot);

            PlatformBuildScene[] scenes = [
                new PlatformBuildScene(
                    Nintendo3DsStartupSceneIds.DirectStartupSceneId,
                    "Demo Disc Main Menu",
                    "scene",
                    [
                        new PlatformBuildPayloadReference(
                            Nintendo3DsStartupSceneIds.DirectStartupSceneCookedRelativePath,
                            Nintendo3DsStartupSceneIds.DirectStartupSceneCookedRelativePath),
                        new PlatformBuildPayloadReference("cooked/engine/materials/standard.hasset", "cooked/engine/materials/standard.hasset")
                    ],
                    [new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, Nintendo3DsStartupSceneIds.DirectStartupSceneCookedRelativePath)])
            ];

            PlatformBuildManifest manifest = new(
                2,
                "project",
                "1.0.0",
                "1.0.0",
                "3ds",
                "1",
                Nintendo3DsStartupSceneIds.DirectStartupSceneId,
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
                selectedBuildOptionValues: new Dictionary<string, string>(StringComparer.Ordinal) {
                    ["startup-top-screen-color"] = "#FF0000",
                    ["startup-bottom-screen-color"] = "#0000FF"
                },
                selectedGraphicsOptionValues: new Dictionary<string, string>(StringComparer.Ordinal),
                selectedCodegenOptionValues: new Dictionary<string, string>(StringComparer.Ordinal),
                generatedCoreCppRootPath: generatedCoreRoot,
                selectedMediaProfileId: "3dsx-homebrew",
                selectedStorageProfileId: "romfs-package");

            var report = await builder.BuildAsync(
                request,
                progressReporter,
                diagnosticReporter,
                CancellationToken.None);

            Assert.True(report.Succeeded);
            string stagedScenePath = Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "DemoDiscMainMenu.hasset");
            string stagedMaterialPath = Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "engine", "materials", "standard.hasset");
            Assert.True(File.Exists(stagedScenePath));
            Assert.True(File.Exists(stagedMaterialPath));
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, true);
            }
        }
    }
}
