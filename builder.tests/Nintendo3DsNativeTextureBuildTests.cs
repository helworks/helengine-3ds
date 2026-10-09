using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.nintendo3ds.builder.tests.Builders;
using System.Runtime.Versioning;

namespace helengine.nintendo3ds.builder.tests;

/// <summary>Exercises native source import, work-item cooking, serialization and RomFS staging through the actual builder.</summary>
[SupportedOSPlatform("windows")]
public sealed class Nintendo3DsNativeTextureBuildTests {
    /// <summary>Ensures texture and generated font-atlas work items retain their compressed PICA encoding in the staged package.</summary>
    [Theory]
    [InlineData("texture")]
    [InlineData("font-atlas-texture")]
    public async Task Native_work_items_retain_gpu_payload_in_romfs(string kind) {
        string repository = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string working = Path.Combine(Path.GetDirectoryName(repository), "builds", "helengine-3ds", "native-texture-tests", Guid.NewGuid().ToString("N"));
        string builderWorking = Path.Combine(working, "builder"), generated = Path.Combine(working, "generated-core");
        string package = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(builderWorking);
        Directory.CreateDirectory(Path.Combine(package, "cooked", "scenes")); Directory.CreateDirectory(generated);
        File.WriteAllText(Path.Combine(generated, "helcpp_config.hpp"), "// Builder orchestration fixture; SDK compilation is validated separately.");
        File.WriteAllBytes(Path.Combine(package, "cooked", "scenes", "main.hasset"), new byte[] { 0x47, 0x42, 0x53 });
        string source = Path.Combine(AppContext.BaseDirectory, "Fixtures", "tex3ds", "rgba8-auto.t3x");
        var settings = new TextureAssetProcessorSettings { MaxResolution = 8, ColorFormatId = "Nintendo3Ds.Etc1A4", AlphaPrecision = TextureAssetAlphaPrecision.A4 };
        PlatformBuildScene scene = new PlatformBuildScene("main", "Main", "scene",
            new[] { new PlatformBuildPayloadReference("cooked/scenes/main.hasset", "cooked/scenes/main.hasset") },
            new[] { new KeyValuePair<string, string>(PlatformBuildSceneMetadataKeys.CookedRelativePath, "cooked/scenes/main.hasset") });
        PlatformCookWorkItem item = new PlatformCookWorkItem("native-texture", source, kind, "3ds", "runtime-texture", "cooked/imported/native.hasset",
            "native-3ds", "source-hash", "settings-hash", Nintendo3DsTextureCookSettingsSerializer.Serialize(settings),
            new[] { new PlatformCookWorkItemMetadata("source-asset-id", "native-3ds") });
        PlatformBuildManifest manifest = new PlatformBuildManifest(1, "project", "1.0.0", "1.0.0", "3ds", "1", "main", new[] { scene },
            Array.Empty<PlatformBuildAsset>(), Array.Empty<PlatformBuildArtifact>(), Array.Empty<PlatformBuildCodeModule>(), Array.Empty<PlatformArtifactPlacement>(),
            new PlatformContainerWritePlan("3ds-romfs-package", Array.Empty<PlatformContainerArtifact>()), new[] { item }, PlatformBuildRuntimeFeatureManifest.Empty);
        PlatformBuildRequest request = new PlatformBuildRequest(manifest,
            new[] { new PlatformBuildTargetVariant("3ds-default", "3ds", "3ds", "3ds-default") },
            new[] { new PlatformCookProfile("3ds-default", "3DS Default", new PlatformCookProfileCapabilities("3ds", "raw", "raw", "3ds-scene-v1", PlatformSerializationEndianness.LittleEndian)) },
            Path.Combine(working, "out"), builderWorking,
            selectedBuildProfileId: "3ds-default", selectedGraphicsProfileId: "3ds-bootstrap", selectedCodegenProfileId: "default",
            selectedBuildOptionValues: new Dictionary<string, string> { ["startup-top-screen-color"] = "#FF0000", ["startup-bottom-screen-color"] = "#0000FF" },
            selectedGraphicsOptionValues: new Dictionary<string, string>(), selectedCodegenOptionValues: new Dictionary<string, string>(),
            generatedCoreCppRootPath: generated, selectedMediaProfileId: "3dsx-homebrew", selectedStorageProfileId: "romfs-package");
        FakeNintendo3DsNativeBuildExecutor executor = new FakeNintendo3DsNativeBuildExecutor();
        Nintendo3DsPlatformAssetBuilder builder = new Nintendo3DsPlatformAssetBuilder(executor, repository);
        await builder.BuildAsync(request, new RecordingProgressReporter(), new RecordingDiagnosticReporter(), CancellationToken.None);
        string staged = Path.Combine(executor.Workspace.RomFsRootPath, "cooked", "imported", "native.hasset");
        using TextureAsset cooked = Assert.IsType<TextureAsset>(helengine.files.AssetSerializer.DeserializeFromBytes(File.ReadAllBytes(staged)));
        Assert.Equal("native-3ds", cooked.Id); Assert.Equal(TextureAssetColorFormat.Nintendo3DsNative, cooked.ColorFormat);
        Assert.Equal(8, cooked.Width); Assert.Equal(8, cooked.Height);
        Assert.Equal(13, Nintendo3DsTextureCodec.ReadLayout(cooked).Format.Code);
        Assert.Equal(96, cooked.Colors.Length);
        using TextureAsset expected = new Nintendo3DsPlatformCookSourceProcessor().CookTexture(source, "native-3ds", settings);
        Assert.Equal(expected.Colors, cooked.Colors);
    }
}
