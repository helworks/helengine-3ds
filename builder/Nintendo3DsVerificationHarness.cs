using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using System.Runtime.Versioning;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Executes builder-owned smoke verification for Nintendo 3DS builds.
/// </summary>
[SupportedOSPlatform("windows")]
internal static class Nintendo3DsVerificationHarness {
    /// <summary>
    /// Executes one builder-owned smoke build using fresh staged inputs and the real native executor.
    /// </summary>
    public static void RunSmokeTest() {
        string repositoryRootPath = Nintendo3DsPlatformAssetBuilder.ResolveRepositoryRootPath();
        RunSmokeTest(
            Path.Combine(Path.GetTempPath(), "helengine-3ds-builder-smoke-" + Guid.NewGuid().ToString("N")),
            Path.Combine(repositoryRootPath, "artifacts", "builder-smoke"),
            repositoryRootPath,
            new Nintendo3DsNativeBuildExecutor());
    }

    /// <summary>
    /// Executes one builder-owned smoke build using an explicit working root and native executor.
    /// </summary>
    /// <param name="workingRootPath">Working root used for the smoke build.</param>
    /// <param name="nativeBuildExecutor">Native build executor used by the smoke build.</param>
    internal static void RunSmokeTest(string workingRootPath, INintendo3DsNativeBuildExecutor nativeBuildExecutor) {
        string repositoryRootPath = Nintendo3DsPlatformAssetBuilder.ResolveRepositoryRootPath();
        RunSmokeTest(workingRootPath, Path.Combine(Path.GetFullPath(workingRootPath), "out"), repositoryRootPath, nativeBuildExecutor);
    }

    /// <summary>
    /// Executes one builder-owned smoke build using explicit working, output, and repository roots.
    /// </summary>
    /// <param name="workingRootPath">Working root used for the smoke build.</param>
    /// <param name="outputRootPath">Output root that should receive the exported package.</param>
    /// <param name="repositoryRootPath">Repository root used for the native Nintendo 3DS build.</param>
    /// <param name="nativeBuildExecutor">Native build executor used by the smoke build.</param>
    internal static void RunSmokeTest(
        string workingRootPath,
        string outputRootPath,
        string repositoryRootPath,
        INintendo3DsNativeBuildExecutor nativeBuildExecutor) {
        if (string.IsNullOrWhiteSpace(workingRootPath)) {
            throw new ArgumentException("Working root path must be provided.", nameof(workingRootPath));
        } else if (string.IsNullOrWhiteSpace(outputRootPath)) {
            throw new ArgumentException("Output root path must be provided.", nameof(outputRootPath));
        } else if (string.IsNullOrWhiteSpace(repositoryRootPath)) {
            throw new ArgumentException("Repository root path must be provided.", nameof(repositoryRootPath));
        } else if (nativeBuildExecutor == null) {
            throw new ArgumentNullException(nameof(nativeBuildExecutor));
        }

        string fullWorkingRootPath = Path.GetFullPath(workingRootPath);
        string fullOutputRootPath = Path.GetFullPath(outputRootPath);
        string fullRepositoryRootPath = Path.GetFullPath(repositoryRootPath);

        try {
            string builderWorkingRootPath = Path.Combine(fullWorkingRootPath, "tmp");
            PrepareSmokeBuildInputs(builderWorkingRootPath);
            Nintendo3DsPlatformAssetBuilder builder = new(nativeBuildExecutor, fullRepositoryRootPath);
            PlatformBuildReport report = builder.BuildAsync(
                CreateBuildRequest(fullOutputRootPath, builderWorkingRootPath),
                new Nintendo3DsNullProgressReporter(),
                new Nintendo3DsNullDiagnosticReporter(),
                CancellationToken.None).GetAwaiter().GetResult();

            if (!report.Succeeded) {
                throw new InvalidOperationException("Smoke test build failed.");
            }

            Console.WriteLine("Smoke test passed.");
            Console.WriteLine(Path.Combine(fullOutputRootPath, "helengine_3ds.3dsx"));
        } finally {
            if (nativeBuildExecutor is Nintendo3DsNativeBuildExecutor) {
                TryDeleteDirectory(fullWorkingRootPath);
            }
        }
    }

    /// <summary>
    /// Creates one 3DS build request for the generated boot-scene smoke run.
    /// </summary>
    /// <param name="outputRootPath">Output root that should receive the exported package.</param>
    /// <param name="workingRootPath">Builder-owned working root used for staged 3DS inputs.</param>
    /// <returns>Prepared build request.</returns>
    static PlatformBuildRequest CreateBuildRequest(string outputRootPath, string workingRootPath) {
        string generatedCoreRootPath = Path.Combine(workingRootPath, "generated-core");
        PlatformBuildScene[] scenes = [
            new PlatformBuildScene(
                Nintendo3DsStartupSceneIds.GeneratedBootSceneId,
                "Generated Boot Scene",
                "scene",
                [new PlatformBuildPayloadReference(
                    Nintendo3DsStartupSceneIds.GeneratedBootSceneCookedRelativePath,
                    Nintendo3DsStartupSceneIds.GeneratedBootSceneCookedRelativePath)],
                [new KeyValuePair<string, string>(
                    PlatformBuildSceneMetadataKeys.CookedRelativePath,
                    Nintendo3DsStartupSceneIds.GeneratedBootSceneCookedRelativePath)])
        ];
        PlatformBuildManifest manifest = new(
            1,
            "project",
            "1.0.0",
            "1.0.0",
            "3ds",
            "1",
            Nintendo3DsStartupSceneIds.GeneratedBootSceneId,
            scenes,
            Array.Empty<PlatformBuildAsset>(),
            Array.Empty<PlatformBuildArtifact>(),
            Array.Empty<PlatformBuildCodeModule>(),
            Array.Empty<PlatformArtifactPlacement>(),
            new PlatformContainerWritePlan("3ds-romfs-package", Array.Empty<PlatformContainerArtifact>()));

        return new PlatformBuildRequest(
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
            outputRootPath,
            workingRootPath,
            selectedBuildProfileId: "3ds-default",
            selectedGraphicsProfileId: "3ds-bootstrap",
            selectedCodegenProfileId: "default",
            selectedBuildOptionValues: new Dictionary<string, string> {
                ["startup-top-screen-color"] = "#FF0000",
                ["startup-bottom-screen-color"] = "#0000FF"
            },
            selectedGraphicsOptionValues: new Dictionary<string, string>(),
            selectedCodegenOptionValues: new Dictionary<string, string>(),
            generatedCoreCppRootPath: generatedCoreRootPath,
            selectedMediaProfileId: "3dsx-homebrew",
            selectedStorageProfileId: "romfs-package");
    }

    /// <summary>
    /// Prepares the minimal generated boot-scene payload required by the builder smoke contract.
    /// </summary>
    /// <param name="workingRootPath">Builder-owned working root that contains the staged package source root.</param>
    static void PrepareSmokeBuildInputs(string workingRootPath) {
        if (string.IsNullOrWhiteSpace(workingRootPath)) {
            throw new ArgumentException("Working root path must be provided.", nameof(workingRootPath));
        }

        string packageSourceRootPath = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(workingRootPath);
        string generatedCoreRootPath = Path.Combine(workingRootPath, "generated-core");
        Directory.CreateDirectory(generatedCoreRootPath);
        File.WriteAllText(Path.Combine(generatedCoreRootPath, "helcpp_config.hpp"), "// smoke");
        string generatedBootScenePath = Path.Combine(
            packageSourceRootPath,
            Nintendo3DsStartupSceneIds.GeneratedBootSceneCookedRelativePath.Replace('/', Path.DirectorySeparatorChar));
        string generatedBootSceneDirectoryPath = Path.GetDirectoryName(generatedBootScenePath)
            ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS smoke startup-scene directory.");
        Directory.CreateDirectory(generatedBootSceneDirectoryPath);
        File.WriteAllBytes(generatedBootScenePath, [0x47, 0x42, 0x53]);
    }

    /// <summary>
    /// Deletes one directory when it exists.
    /// </summary>
    /// <param name="path">Directory path to delete.</param>
    static void TryDeleteDirectory(string path) {
        if (!string.IsNullOrWhiteSpace(path) && Directory.Exists(path)) {
            Directory.Delete(path, recursive: true);
        }
    }
}
