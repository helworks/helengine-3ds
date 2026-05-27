using System.Globalization;
using System.Runtime.Versioning;
using helengine;
using helengine.baseplatform.Builders;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.files;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Implements the Nintendo 3DS platform asset builder contract.
/// </summary>
[SupportedOSPlatform("windows")]
public sealed class Nintendo3DsPlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>
    /// Environment variable that can override the Nintendo 3DS repository root.
    /// </summary>
    const string RepositoryRootEnvironmentVariableName = "HELENGINE_3DS_REPOSITORY_ROOT";

    /// <summary>
    /// Stores the native build executor seam used by build orchestration.
    /// </summary>
    readonly INintendo3DsNativeBuildExecutor NativeBuildExecutor;

    /// <summary>
    /// Stores the resolved Nintendo 3DS repository root used for native packaging.
    /// </summary>
    readonly string RepositoryRootPath;

    /// <summary>
    /// Stores the builder-owned startup-manifest writer used to stage RomFS payloads.
    /// </summary>
    readonly Nintendo3DsStartupManifestWriter StartupManifestWriter;

    /// <summary>
    /// Stores the staged RomFS asset copier used to mirror cooked payload files into the runtime tree.
    /// </summary>
    readonly Nintendo3DsRomFsAssetStager RomFsAssetStager;

    /// <summary>
    /// Imports source textures and converts them into final runtime payloads for builder-owned Nintendo 3DS cook work items.
    /// </summary>
    readonly INintendo3DsPlatformCookSourceProcessor PlatformCookSourceProcessor;

    /// <summary>
    /// Initializes one Nintendo 3DS builder with the default native build executor.
    /// </summary>
    public Nintendo3DsPlatformAssetBuilder() {
        NativeBuildExecutor = new Nintendo3DsNativeBuildExecutor();
        RepositoryRootPath = string.Empty;
        StartupManifestWriter = new Nintendo3DsStartupManifestWriter();
        RomFsAssetStager = new Nintendo3DsRomFsAssetStager();
        PlatformCookSourceProcessor = new Nintendo3DsPlatformCookSourceProcessor();
        Descriptor = CreateDescriptor();
        Definition = Nintendo3DsPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Initializes one Nintendo 3DS builder with a custom native build executor for tests.
    /// </summary>
    /// <param name="nativeBuildExecutor">Native build executor override used by tests.</param>
    /// <param name="repositoryRootPath">Repository root override used by tests.</param>
    internal Nintendo3DsPlatformAssetBuilder(INintendo3DsNativeBuildExecutor nativeBuildExecutor, string repositoryRootPath) {
        NativeBuildExecutor = nativeBuildExecutor ?? throw new ArgumentNullException(nameof(nativeBuildExecutor));
        RepositoryRootPath = string.IsNullOrWhiteSpace(repositoryRootPath)
            ? throw new ArgumentException("Repository root path must be provided.", nameof(repositoryRootPath))
            : Path.GetFullPath(repositoryRootPath);
        StartupManifestWriter = new Nintendo3DsStartupManifestWriter();
        RomFsAssetStager = new Nintendo3DsRomFsAssetStager();
        PlatformCookSourceProcessor = new Nintendo3DsPlatformCookSourceProcessor();
        Descriptor = CreateDescriptor();
        Definition = Nintendo3DsPlatformDefinitionFactory.Create();
    }

    /// <summary>
    /// Gets the explicit builder descriptor exposed to the editor.
    /// </summary>
    public PlatformBuilderDescriptor Descriptor { get; }

    /// <summary>
    /// Gets the typed Nintendo 3DS platform definition exposed to the editor.
    /// </summary>
    public PlatformDefinition Definition { get; }

    /// <summary>
    /// Cooks one Nintendo 3DS fixed-pipeline material payload for the generic cooked-platform-owned runtime seam.
    /// </summary>
    /// <param name="request">Material cook request to translate.</param>
    /// <returns>Cooked Nintendo 3DS material payload.</returns>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (!string.Equals(request.TargetPlatformId, "3ds", StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException($"Nintendo 3DS cannot cook materials for target platform '{request.TargetPlatformId}'.");
        } else if (!string.Equals(request.SchemaId, Nintendo3DsMaterialSchemaIds.StandardTexturedSchemaId, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException($"Nintendo 3DS does not support material schema '{request.SchemaId}'.");
        }

        ResolveBaseColor(
            request.FieldValues,
            out byte baseColorRed,
            out byte baseColorGreen,
            out byte baseColorBlue,
            out byte baseColorAlpha);
        PlatformMaterialAsset cookedAsset = new PlatformMaterialAsset {
            Id = request.MaterialAssetId,
            RendererFamilyId = string.IsNullOrWhiteSpace(request.SelectedGraphicsProfileId)
                ? throw new InvalidOperationException("Nintendo 3DS material cooking requires a graphics profile id.")
                : request.SelectedGraphicsProfileId,
            TextureRelativePath = ResolveOptionalString(request.FieldValues, Nintendo3DsMaterialSchemaIds.TextureRelativePathFieldId),
            DoubleSided = ResolveBoolean(request.FieldValues, Nintendo3DsMaterialSchemaIds.DoubleSidedFieldId),
            UseVertexColor = ResolveVertexColorMode(request.FieldValues),
            Lit = ResolveLightingMode(request.FieldValues),
            BaseColorR = baseColorRed,
            BaseColorG = baseColorGreen,
            BaseColorB = baseColorBlue,
            BaseColorA = baseColorAlpha
        };

        return new PlatformMaterialCookResult(
            helengine.files.AssetSerializer.SerializeToBytes(cookedAsset),
            []);
    }

    /// <summary>
    /// Writes the 3DS startup manifest into RomFS and invokes the native packaging flow.
    /// </summary>
    /// <param name="request">Build request to process.</param>
    /// <param name="progressReporter">Progress reporter that receives streamed updates.</param>
    /// <param name="diagnosticReporter">Diagnostic reporter that receives streamed diagnostics.</param>
    /// <param name="cancellationToken">Cancellation token used to stop the build cooperatively.</param>
    /// <returns>Final Nintendo 3DS build report for the supplied request.</returns>
    public async Task<PlatformBuildReport> BuildAsync(
        PlatformBuildRequest request,
        IPlatformBuildProgressReporter progressReporter,
        IPlatformBuildDiagnosticReporter diagnosticReporter,
        CancellationToken cancellationToken) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (progressReporter == null) {
            throw new ArgumentNullException(nameof(progressReporter));
        } else if (diagnosticReporter == null) {
            throw new ArgumentNullException(nameof(diagnosticReporter));
        } else if (string.IsNullOrWhiteSpace(request.GeneratedCoreCppRootPath)) {
            throw new ArgumentException("Generated core root path must be provided for Nintendo 3DS builds.", nameof(request));
        }

        string topScreenColor = ReadRequiredBuildOption(request.SelectedBuildOptionValues, "startup-top-screen-color");
        string bottomScreenColor = ReadRequiredBuildOption(request.SelectedBuildOptionValues, "startup-bottom-screen-color");
        string repositoryRootPath = string.IsNullOrWhiteSpace(RepositoryRootPath)
            ? ResolveRepositoryRootPath()
            : RepositoryRootPath;

        Directory.CreateDirectory(request.OutputRoot);
        Directory.CreateDirectory(request.WorkingRoot);

        Nintendo3DsBuildWorkspace workspace = Nintendo3DsBuildWorkspace.Create(
            repositoryRootPath,
            request.WorkingRoot,
            request.OutputRoot,
            request.GeneratedCoreCppRootPath);
        PlatformBuildScene effectiveStartupScene = ResolveEffectiveStartupScene(request.Manifest);
        string packageSourceRootPath = Nintendo3DsBuildPathConventions.ResolvePackageSourceRootPath(request.WorkingRoot);
        ValidatePackageSourceRootPath(packageSourceRootPath);

        ExecutePlatformCookWorkItems(request, packageSourceRootPath, progressReporter, cancellationToken);
        ResetDirectory(workspace.RomFsRootPath);
        RomFsAssetStager.Stage(request.Manifest, packageSourceRootPath, workspace.RomFsRootPath);
        progressReporter.Report(new PlatformBuildProgressUpdate(
            "Stage Packaged Content",
            ReadCookedRelativePath(effectiveStartupScene),
            1,
            3,
            "Staged Nintendo 3DS packaged payloads into the RomFS root."));
        StartupManifestWriter.Write(workspace.RomFsRootPath, topScreenColor, bottomScreenColor);
        progressReporter.Report(new PlatformBuildProgressUpdate(
            "Write Startup Manifest",
            "runtime/3ds_startup_manifest.bin",
            2,
            3,
            "Wrote the 3DS startup manifest into the staged RomFS root."));

        NativeBuildExecutor.Build(workspace, cancellationToken);
        progressReporter.Report(new PlatformBuildProgressUpdate(
            "Build Nintendo 3DS Package",
            "helengine_3ds.3dsx",
            3,
            3,
            "Built the Nintendo 3DS package."));

        PlatformBuildReport report = new(
            File.Exists(workspace.ExportPackagePath),
            [],
            BuildSceneOutcomes(request.Manifest.Scenes),
            BuildLooseAssetOutcomes(request.Manifest.LooseAssets));
        return await Task.FromResult(report);
    }

    /// <summary>
    /// Creates the standard builder descriptor shared by both constructors.
    /// </summary>
    /// <returns>The Nintendo 3DS builder descriptor.</returns>
    static PlatformBuilderDescriptor CreateDescriptor() {
        return new PlatformBuilderDescriptor(
            "helengine.3ds.builder",
            "1.0.0",
            "3ds",
            new EngineCompatibilityRange("1.0.0", "999.0.0"),
            new ManifestCompatibilityRange(1, 3),
            ["3ds"],
            ["3ds-default"]);
    }

    /// <summary>
    /// Resolves the generated boot scene that Nintendo 3DS builds must use as the effective startup scene.
    /// </summary>
    /// <param name="manifest">Resolved build manifest supplied by the editor.</param>
    /// <returns>Resolved generated boot-scene entry.</returns>
    static PlatformBuildScene ResolveEffectiveStartupScene(PlatformBuildManifest manifest) {
        if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        }

        for (int index = 0; index < manifest.Scenes.Length; index++) {
            PlatformBuildScene scene = manifest.Scenes[index];
            if (!string.Equals(scene.SceneId, Nintendo3DsStartupSceneIds.GeneratedBootSceneId, StringComparison.Ordinal)) {
                continue;
            }

            string cookedRelativePath = ReadCookedRelativePath(scene);
            if (!string.Equals(
                cookedRelativePath,
                Nintendo3DsStartupSceneIds.GeneratedBootSceneCookedRelativePath,
                StringComparison.Ordinal)) {
                throw new InvalidOperationException(
                    $"Nintendo 3DS requires startup scene '{Nintendo3DsStartupSceneIds.GeneratedBootSceneId}' to use cooked path '{Nintendo3DsStartupSceneIds.GeneratedBootSceneCookedRelativePath}'.");
            }

            return scene;
        }

        throw new InvalidOperationException($"Nintendo 3DS builds require {Nintendo3DsStartupSceneIds.GeneratedBootSceneId}.");
    }

    /// <summary>
    /// Reads one required Nintendo 3DS build option value.
    /// </summary>
    /// <param name="values">Selected build option values keyed by option id.</param>
    /// <param name="key">Build option id to resolve.</param>
    /// <returns>Resolved non-empty build option value.</returns>
    static string ReadRequiredBuildOption(IReadOnlyDictionary<string, string> values, string key) {
        if (values == null) {
            throw new ArgumentNullException(nameof(values));
        } else if (string.IsNullOrWhiteSpace(key)) {
            throw new ArgumentException("Build option id must be provided.", nameof(key));
        }

        if (!values.TryGetValue(key, out string value) || string.IsNullOrWhiteSpace(value)) {
            throw new InvalidOperationException($"Missing required Nintendo 3DS build option '{key}'.");
        }

        return value;
    }

    /// <summary>
    /// Reads one required boolean field from the material cook request.
    /// </summary>
    /// <param name="fieldValues">Material field values keyed by field id.</param>
    /// <param name="fieldId">Field identifier to resolve.</param>
    /// <returns>Resolved boolean value.</returns>
    static bool ResolveBoolean(IReadOnlyDictionary<string, string> fieldValues, string fieldId) {
        string value = ResolveRequiredString(fieldValues, fieldId);
        if (string.Equals(value, "true", StringComparison.OrdinalIgnoreCase)) {
            return true;
        } else if (string.Equals(value, "false", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        throw new InvalidOperationException($"Nintendo 3DS material field '{fieldId}' must be 'true' or 'false'.");
    }

    /// <summary>
    /// Resolves whether vertex colors should modulate the final output color.
    /// </summary>
    /// <param name="fieldValues">Material field values keyed by field id.</param>
    /// <returns>True when vertex color modulation should remain enabled.</returns>
    static bool ResolveVertexColorMode(IReadOnlyDictionary<string, string> fieldValues) {
        string value = ResolveRequiredString(fieldValues, Nintendo3DsMaterialSchemaIds.VertexColorModeFieldId);
        if (string.Equals(value, "multiply", StringComparison.OrdinalIgnoreCase)) {
            return true;
        } else if (string.Equals(value, "ignore", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        throw new InvalidOperationException("Nintendo 3DS material field 'vertex-color-mode' must be 'multiply' or 'ignore'.");
    }

    /// <summary>
    /// Resolves whether the cooked material should participate in lighting.
    /// </summary>
    /// <param name="fieldValues">Material field values keyed by field id.</param>
    /// <returns>True when lighting should remain enabled.</returns>
    static bool ResolveLightingMode(IReadOnlyDictionary<string, string> fieldValues) {
        string value = ResolveRequiredString(fieldValues, Nintendo3DsMaterialSchemaIds.LightingModeFieldId);
        if (string.Equals(value, "lit", StringComparison.OrdinalIgnoreCase)) {
            return true;
        } else if (string.Equals(value, "unlit", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        throw new InvalidOperationException("Nintendo 3DS material field 'lighting-mode' must be 'lit' or 'unlit'.");
    }

    /// <summary>
    /// Resolves one optional string field from the material cook request.
    /// </summary>
    /// <param name="fieldValues">Material field values keyed by field id.</param>
    /// <param name="fieldId">Field identifier to resolve.</param>
    /// <returns>Resolved string value, or an empty string when the field is absent.</returns>
    static string ResolveOptionalString(IReadOnlyDictionary<string, string> fieldValues, string fieldId) {
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (string.IsNullOrWhiteSpace(fieldId)) {
            throw new ArgumentException("Field id must be provided.", nameof(fieldId));
        }

        if (!fieldValues.TryGetValue(fieldId, out string value) || string.IsNullOrWhiteSpace(value)) {
            return string.Empty;
        }

        return value;
    }

    /// <summary>
    /// Resolves one required string field from the material cook request.
    /// </summary>
    /// <param name="fieldValues">Material field values keyed by field id.</param>
    /// <param name="fieldId">Field identifier to resolve.</param>
    /// <returns>Resolved non-empty field value.</returns>
    static string ResolveRequiredString(IReadOnlyDictionary<string, string> fieldValues, string fieldId) {
        if (fieldValues == null) {
            throw new ArgumentNullException(nameof(fieldValues));
        } else if (string.IsNullOrWhiteSpace(fieldId)) {
            throw new ArgumentException("Field id must be provided.", nameof(fieldId));
        }

        if (!fieldValues.TryGetValue(fieldId, out string value) || string.IsNullOrWhiteSpace(value)) {
            throw new InvalidOperationException($"Nintendo 3DS material field '{fieldId}' is required.");
        }

        return value;
    }

    /// <summary>
    /// Resolves one HTML-style base color value into packed byte channels.
    /// </summary>
    /// <param name="fieldValues">Material field values keyed by field id.</param>
    /// <param name="red">Resolved red channel.</param>
    /// <param name="green">Resolved green channel.</param>
    /// <param name="blue">Resolved blue channel.</param>
    /// <param name="alpha">Resolved alpha channel.</param>
    static void ResolveBaseColor(
        IReadOnlyDictionary<string, string> fieldValues,
        out byte red,
        out byte green,
        out byte blue,
        out byte alpha) {
        string value = ResolveRequiredString(fieldValues, Nintendo3DsMaterialSchemaIds.BaseColorFieldId);
        if (value.Length == 7 && value[0] == '#') {
            red = ParseHexByte(value, 1);
            green = ParseHexByte(value, 3);
            blue = ParseHexByte(value, 5);
            alpha = 255;
            return;
        } else if (value.Length == 9 && value[0] == '#') {
            red = ParseHexByte(value, 1);
            green = ParseHexByte(value, 3);
            blue = ParseHexByte(value, 5);
            alpha = ParseHexByte(value, 7);
            return;
        }

        throw new InvalidOperationException("Nintendo 3DS material field 'base-color' must use #RRGGBB or #RRGGBBAA.");
    }

    /// <summary>
    /// Parses one hexadecimal byte from a serialized color string.
    /// </summary>
    /// <param name="value">Serialized color string.</param>
    /// <param name="startIndex">Zero-based character index of the first hexadecimal digit.</param>
    /// <returns>Resolved byte value.</returns>
    static byte ParseHexByte(string value, int startIndex) {
        if (string.IsNullOrWhiteSpace(value)) {
            throw new ArgumentException("Color value must be provided.", nameof(value));
        }

        string segment = value.Substring(startIndex, 2);
        if (byte.TryParse(segment, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out byte parsedValue)) {
            return parsedValue;
        }

        throw new InvalidOperationException($"Nintendo 3DS material color value '{value}' contains invalid hexadecimal digits.");
    }

    /// <summary>
    /// Reads the cooked runtime-relative payload path stored on one resolved scene entry.
    /// </summary>
    /// <param name="scene">Resolved scene entry whose cooked payload path should be read.</param>
    /// <returns>Cooked runtime-relative payload path.</returns>
    static string ReadCookedRelativePath(PlatformBuildScene scene) {
        if (scene == null) {
            throw new ArgumentNullException(nameof(scene));
        }

        for (int index = 0; index < scene.ResolvedMetadata.Length; index++) {
            KeyValuePair<string, string> metadataEntry = scene.ResolvedMetadata[index];
            if (!string.Equals(metadataEntry.Key, PlatformBuildSceneMetadataKeys.CookedRelativePath, StringComparison.Ordinal)) {
                continue;
            }

            if (string.IsNullOrWhiteSpace(metadataEntry.Value)) {
                break;
            }

            return NormalizeRelativePath(metadataEntry.Value);
        }

        throw new InvalidOperationException(
            $"Nintendo 3DS scene '{scene.SceneId}' did not define the cooked-relative payload path metadata.");
    }

    /// <summary>
    /// Resolves the Nintendo 3DS repository root from either the environment or the builder assembly location.
    /// </summary>
    /// <returns>Resolved Nintendo 3DS repository root path.</returns>
    internal static string ResolveRepositoryRootPath() {
        string environmentOverride = Environment.GetEnvironmentVariable(RepositoryRootEnvironmentVariableName);
        if (!string.IsNullOrWhiteSpace(environmentOverride)) {
            return Path.GetFullPath(environmentOverride);
        }

        string assemblyDirectoryPath = Path.GetDirectoryName(typeof(Nintendo3DsPlatformAssetBuilder).Assembly.Location)
            ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS builder assembly directory.");
        return ResolveRepositoryRootPath(assemblyDirectoryPath);
    }

    /// <summary>
    /// Walks upward from one builder assembly directory until it finds the Nintendo 3DS repository markers.
    /// </summary>
    /// <param name="assemblyDirectoryPath">Builder assembly directory used as the starting point.</param>
    /// <returns>Resolved Nintendo 3DS repository root path.</returns>
    internal static string ResolveRepositoryRootPath(string assemblyDirectoryPath) {
        if (string.IsNullOrWhiteSpace(assemblyDirectoryPath)) {
            throw new ArgumentException("Assembly directory path must be provided.", nameof(assemblyDirectoryPath));
        }

        DirectoryInfo currentDirectory = new DirectoryInfo(Path.GetFullPath(assemblyDirectoryPath));
        while (currentDirectory != null) {
            string candidateRepositoryRootPath = currentDirectory.FullName;
            if (File.Exists(Path.Combine(candidateRepositoryRootPath, "Makefile")) &&
                File.Exists(Path.Combine(candidateRepositoryRootPath, "platform-plugin.json"))) {
                return candidateRepositoryRootPath;
            }

            currentDirectory = currentDirectory.Parent;
        }

        throw new InvalidOperationException("Unable to resolve the Nintendo 3DS repository root from the builder assembly directory.");
    }

    /// <summary>
    /// Verifies the builder-owned package-source root exists before RomFS staging begins.
    /// </summary>
    /// <param name="packageSourceRootPath">Resolved builder-owned package-source root.</param>
    static void ValidatePackageSourceRootPath(string packageSourceRootPath) {
        if (string.IsNullOrWhiteSpace(packageSourceRootPath)) {
            throw new ArgumentException("Package source root path must be provided.", nameof(packageSourceRootPath));
        } else if (!Directory.Exists(packageSourceRootPath)) {
            throw new DirectoryNotFoundException($"Nintendo 3DS package source root '{packageSourceRootPath}' was not found.");
        }
    }

    /// <summary>
    /// Recreates one directory from scratch.
    /// </summary>
    /// <param name="path">Directory path to recreate.</param>
    static void ResetDirectory(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            throw new ArgumentException("Directory path must be provided.", nameof(path));
        }

        string fullPath = Path.GetFullPath(path);
        if (Directory.Exists(fullPath)) {
            Directory.Delete(fullPath, recursive: true);
        }

        Directory.CreateDirectory(fullPath);
    }

    /// <summary>
    /// Executes builder-owned Nintendo 3DS cook work items directly into the staged package-source root using the editor-resolved source path and serialized settings payload.
    /// </summary>
    /// <param name="request">Resolved build request.</param>
    /// <param name="packageSourceRootPath">Builder-owned staged package-source root that receives builder-owned cooked outputs.</param>
    /// <param name="progressReporter">Streaming progress reporter.</param>
    /// <param name="cancellationToken">Cancellation token.</param>
    void ExecutePlatformCookWorkItems(
        PlatformBuildRequest request,
        string packageSourceRootPath,
        IPlatformBuildProgressReporter progressReporter,
        CancellationToken cancellationToken) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (string.IsNullOrWhiteSpace(packageSourceRootPath)) {
            throw new ArgumentException("Package source root path must be provided.", nameof(packageSourceRootPath));
        } else if (progressReporter == null) {
            throw new ArgumentNullException(nameof(progressReporter));
        }

        PlatformCookWorkItem[] platformCookWorkItems = request.Manifest.PlatformCookWorkItems ?? [];
        for (int workItemIndex = 0; workItemIndex < platformCookWorkItems.Length; workItemIndex++) {
            cancellationToken.ThrowIfCancellationRequested();

            PlatformCookWorkItem workItem = platformCookWorkItems[workItemIndex];
            ExecutePlatformCookWorkItem(workItem, packageSourceRootPath);
            progressReporter.Report(new PlatformBuildProgressUpdate(
                "Execute Platform Cook Work Items",
                workItem.OutputLogicalArtifactId,
                workItemIndex + 1,
                platformCookWorkItems.Length,
                $"Cooked platform-owned artifact '{workItem.OutputRelativePath}'."));
        }
    }

    /// <summary>
    /// Executes one builder-owned Nintendo 3DS cook work item and writes the final runtime payload into the exact declared output path.
    /// </summary>
    /// <param name="workItem">Builder-owned Nintendo 3DS cook work item to execute.</param>
    /// <param name="packageSourceRootPath">Builder-owned staged package-source root that receives the cooked output.</param>
    void ExecutePlatformCookWorkItem(PlatformCookWorkItem workItem, string packageSourceRootPath) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        } else if (string.IsNullOrWhiteSpace(packageSourceRootPath)) {
            throw new ArgumentException("Package source root path must be provided.", nameof(packageSourceRootPath));
        } else if (!string.Equals(workItem.TargetPlatformId, Definition.PlatformId, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException($"Unsupported Nintendo 3DS work item target platform '{workItem.TargetPlatformId}'.");
        }

        TextureAssetProcessorSettings settings = Nintendo3DsTextureCookSettingsSerializer.Deserialize(workItem.SerializedPlatformSettings);
        string assetId = ResolveCookWorkItemAssetId(workItem);
        string destinationPath = Path.Combine(packageSourceRootPath, NormalizeRelativePath(workItem.OutputRelativePath).Replace('/', Path.DirectorySeparatorChar));
        string destinationDirectoryPath = Path.GetDirectoryName(destinationPath)
            ?? throw new InvalidOperationException($"Destination directory could not be resolved for '{destinationPath}'.");
        Directory.CreateDirectory(destinationDirectoryPath);

        if (string.Equals(workItem.SourceAssetKind, "texture", StringComparison.OrdinalIgnoreCase)) {
            TextureAsset cookedTextureAsset = PlatformCookSourceProcessor.CookTexture(workItem.SourceAssetPath, assetId, settings);
            File.WriteAllBytes(destinationPath, global::helengine.files.AssetSerializer.SerializeToBytes(cookedTextureAsset));
            return;
        } else if (string.Equals(workItem.SourceAssetKind, "font-atlas-texture", StringComparison.OrdinalIgnoreCase)) {
            string fontAtlasSourcePath = ResolveFontAtlasCookSourcePath(workItem, packageSourceRootPath);
            TextureAsset cookedTextureAsset = PlatformCookSourceProcessor.CookFontAtlasTexture(fontAtlasSourcePath, assetId, settings);
            File.WriteAllBytes(destinationPath, global::helengine.files.AssetSerializer.SerializeToBytes(cookedTextureAsset));
            return;
        }

        throw new InvalidOperationException($"Unsupported Nintendo 3DS platform cook work item source kind '{workItem.SourceAssetKind}'.");
    }

    /// <summary>
    /// Resolves the stable runtime asset id the builder should assign to one builder-owned cooked output.
    /// </summary>
    /// <param name="workItem">Builder-owned Nintendo 3DS cook work item whose metadata should be interpreted.</param>
    /// <returns>Stable runtime asset identifier for the cooked payload.</returns>
    static string ResolveCookWorkItemAssetId(PlatformCookWorkItem workItem) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        }

        PlatformCookWorkItemMetadata[] metadata = workItem.Metadata ?? [];
        for (int index = 0; index < metadata.Length; index++) {
            PlatformCookWorkItemMetadata entry = metadata[index];
            if (entry == null) {
                continue;
            }

            if (string.Equals(entry.Key, "source-asset-id", StringComparison.OrdinalIgnoreCase) && !string.IsNullOrWhiteSpace(entry.Value)) {
                return entry.Value;
            }
        }

        return workItem.OutputRelativePath;
    }

    /// <summary>
    /// Resolves the preferred source path for one font-atlas cook work item.
    /// </summary>
    /// <param name="workItem">Builder-owned Nintendo 3DS cook work item to execute.</param>
    /// <param name="packageSourceRootPath">Builder-owned staged package-source root that may already contain the packaged font asset.</param>
    /// <returns>Preferred source path for atlas cooking.</returns>
    static string ResolveFontAtlasCookSourcePath(PlatformCookWorkItem workItem, string packageSourceRootPath) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        } else if (string.IsNullOrWhiteSpace(packageSourceRootPath)) {
            throw new ArgumentException("Package source root path must be provided.", nameof(packageSourceRootPath));
        } else if (string.IsNullOrWhiteSpace(workItem.SourceAssetPath)) {
            throw new InvalidOperationException("Nintendo 3DS font-atlas work items must provide a source asset path.");
        } else if (string.IsNullOrWhiteSpace(workItem.OutputRelativePath)) {
            throw new InvalidOperationException("Nintendo 3DS font-atlas work items must provide an output relative path.");
        }

        string packagedFontRelativePath = Path.ChangeExtension(NormalizeRelativePath(workItem.OutputRelativePath), ".hefont");
        string packagedFontPath = Path.Combine(packageSourceRootPath, packagedFontRelativePath.Replace('/', Path.DirectorySeparatorChar));
        if (PackagedFontAssetProvidesEmbeddedAtlasTexture(packagedFontPath)) {
            return packagedFontPath;
        }

        return workItem.SourceAssetPath;
    }

    /// <summary>
    /// Determines whether one staged packaged font still contains embedded source-atlas texture bytes.
    /// </summary>
    /// <param name="packagedFontPath">Absolute staged packaged font path to inspect.</param>
    /// <returns>True when the staged packaged font exposes one embedded source atlas texture payload; otherwise false.</returns>
    static bool PackagedFontAssetProvidesEmbeddedAtlasTexture(string packagedFontPath) {
        if (string.IsNullOrWhiteSpace(packagedFontPath)) {
            throw new ArgumentException("Packaged font path must be provided.", nameof(packagedFontPath));
        }
        if (!File.Exists(packagedFontPath)) {
            return false;
        }

        using FileStream stream = new(packagedFontPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        FontAsset fontAsset = global::helengine.files.FontAssetBinarySerializer.Deserialize(stream);
        return fontAsset.SourceTextureAsset != null;
    }

    /// <summary>
    /// Normalizes one payload path to a forward-slash relative path.
    /// </summary>
    /// <param name="path">Payload path to normalize.</param>
    /// <returns>Normalized forward-slash relative path.</returns>
    static string NormalizeRelativePath(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            throw new InvalidOperationException("Nintendo 3DS payload paths must be provided.");
        }

        return CanonicalPackagedAssetPath.ValidateCanonical(path);
    }

    /// <summary>
    /// Builds successful scene outcomes for the completed request.
    /// </summary>
    /// <param name="scenes">Scenes included in the build request.</param>
    /// <returns>Final scene outcomes.</returns>
    static PlatformBuildItemOutcome[] BuildSceneOutcomes(helengine.baseplatform.Manifest.PlatformBuildScene[] scenes) {
        if (scenes == null) {
            throw new ArgumentNullException(nameof(scenes));
        }

        PlatformBuildItemOutcome[] outcomes = new PlatformBuildItemOutcome[scenes.Length];
        for (int index = 0; index < scenes.Length; index++) {
            outcomes[index] = new PlatformBuildItemOutcome(scenes[index].SceneId, PlatformBuildItemOutcomeKind.Succeeded);
        }

        return outcomes;
    }

    /// <summary>
    /// Builds successful loose-asset outcomes for the completed request.
    /// </summary>
    /// <param name="looseAssets">Loose assets included in the build request.</param>
    /// <returns>Final loose-asset outcomes.</returns>
    static PlatformBuildItemOutcome[] BuildLooseAssetOutcomes(helengine.baseplatform.Manifest.PlatformBuildAsset[] looseAssets) {
        if (looseAssets == null) {
            throw new ArgumentNullException(nameof(looseAssets));
        }

        PlatformBuildItemOutcome[] outcomes = new PlatformBuildItemOutcome[looseAssets.Length];
        for (int index = 0; index < looseAssets.Length; index++) {
            outcomes[index] = new PlatformBuildItemOutcome(looseAssets[index].AssetId, PlatformBuildItemOutcomeKind.Succeeded);
        }

        return outcomes;
    }
}
