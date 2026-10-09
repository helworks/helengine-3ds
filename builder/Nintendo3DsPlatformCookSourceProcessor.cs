using System.Runtime.Versioning;
using FilesAssetSerializer = helengine.files.AssetSerializer;
using FilesEngineBinaryHeaderSerializer = helengine.files.EngineBinaryHeaderSerializer;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Imports source textures through Nintendo 3DS-local decode services for builder-owned cook work items.
/// </summary>
[SupportedOSPlatform("windows")]
public sealed class Nintendo3DsPlatformCookSourceProcessor : INintendo3DsPlatformCookSourceProcessor {
    /// <summary>
    /// Stores the source image decoder used for builder-owned texture work items.
    /// </summary>
    readonly Nintendo3DsSourceTextureDecoder SourceTextureDecoder;

    /// <summary>
    /// Stores the raw source font atlas importer used for builder-owned font-atlas work items.
    /// </summary>
    readonly Nintendo3DsSourceFontAtlasTextureImporter SourceFontAtlasTextureImporter;

    /// <summary>
    /// Stores the packaged font extension understood by the builder-owned font-atlas cook path.
    /// </summary>
    readonly string SupportedPackagedFontExtension;

    /// <summary>
    /// Initializes the shared source processor used by Nintendo 3DS builder-owned texture work items.
    /// </summary>
    public Nintendo3DsPlatformCookSourceProcessor() {
        SourceTextureDecoder = new Nintendo3DsSourceTextureDecoder();
        SourceFontAtlasTextureImporter = new Nintendo3DsSourceFontAtlasTextureImporter();
        SupportedPackagedFontExtension = ".hefont";
    }

    /// <summary>
    /// Imports one source texture asset and applies the resolved Nintendo 3DS texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked texture should preserve.</param>
    /// <param name="settings">Resolved Nintendo 3DS texture processor settings supplied by the build graph.</param>
    /// <returns>Processed runtime texture payload ready for serialization.</returns>
    public TextureAsset CookTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        } else if (string.IsNullOrWhiteSpace(assetId)) {
            throw new ArgumentException("Texture asset id must be provided.", nameof(assetId));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        }

        using TextureAsset decodedTextureAsset = LoadTextureSourceAsset(sourceAssetPath);
        TextureAsset cookedTextureAsset = Nintendo3DsTextureCooker.Apply(decodedTextureAsset, settings);
        cookedTextureAsset.Id = assetId;
        cookedTextureAsset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(assetId);
        return cookedTextureAsset;
    }

    /// <summary>
    /// Imports one packaged font asset and applies the resolved Nintendo 3DS atlas texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked atlas texture should preserve.</param>
    /// <param name="settings">Resolved Nintendo 3DS texture processor settings supplied by the build graph.</param>
    /// <returns>Processed runtime atlas texture payload ready for serialization.</returns>
    public TextureAsset CookFontAtlasTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source font path must be provided.", nameof(sourceAssetPath));
        } else if (string.IsNullOrWhiteSpace(assetId)) {
            throw new ArgumentException("Font atlas asset id must be provided.", nameof(assetId));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        } else if (!File.Exists(sourceAssetPath)) {
            throw new FileNotFoundException("Source font file was not found.", sourceAssetPath);
        }

        TextureAsset sourceTextureAsset = TryLoadPackagedFontSourceTextureAsset(sourceAssetPath);
        if (sourceTextureAsset == null) {
            sourceTextureAsset = TryLoadGeneratedFontAtlasTextureAsset(sourceAssetPath);
        }
        if (sourceTextureAsset == null) {
            sourceTextureAsset = SourceFontAtlasTextureImporter.Import(sourceAssetPath);
        }

        using (sourceTextureAsset) {
            TextureAsset cookedTextureAsset = Nintendo3DsTextureCooker.Apply(sourceTextureAsset, settings);
            cookedTextureAsset.Id = assetId;
            cookedTextureAsset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(assetId);
            return cookedTextureAsset;
        }
    }

    /// <summary>
    /// Attempts to load the embedded raw atlas texture from one packaged font asset path.
    /// </summary>
    /// <param name="sourceAssetPath">Candidate packaged font asset path.</param>
    /// <returns>Embedded raw atlas texture when the path references one packaged font asset; otherwise null.</returns>
    TextureAsset TryLoadPackagedFontSourceTextureAsset(string sourceAssetPath) {
        if (!string.Equals(Path.GetExtension(sourceAssetPath), SupportedPackagedFontExtension, StringComparison.OrdinalIgnoreCase)) {
            return null;
        }

        using FileStream stream = new(sourceAssetPath, FileMode.Open, FileAccess.Read, FileShare.Read);
        using FontAsset fontAsset = global::helengine.files.FontAssetBinarySerializer.Deserialize(stream);
        TextureAsset source = fontAsset.SourceTextureAsset;
        fontAsset.SourceTextureAsset = null;
        return source;
    }

    /// <summary>
    /// Attempts to load one editor-generated atlas texture asset before falling back to raw font import.
    /// </summary>
    /// <param name="sourceAssetPath">Candidate generated atlas texture path emitted by the editor build graph.</param>
    /// <returns>Generated texture asset when the path resolves to one serialized texture or source bitmap; otherwise null.</returns>
    TextureAsset TryLoadGeneratedFontAtlasTextureAsset(string sourceAssetPath) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source asset path must be provided.", nameof(sourceAssetPath));
        }

        string extension = Path.GetExtension(sourceAssetPath);
        if (!string.Equals(extension, ".hetex", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".hasset", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".png", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".tga", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".jpg", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".jpeg", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".bmp", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".t3x", StringComparison.OrdinalIgnoreCase)) {
            return null;
        }

        return LoadTextureSourceAsset(sourceAssetPath);
    }

    /// <summary>
    /// Loads one serialized texture asset or decodes one authored bitmap path into one uncooked RGBA texture asset.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path emitted by the editor build graph.</param>
    /// <returns>Resolved uncooked texture asset.</returns>
    TextureAsset LoadTextureSourceAsset(string sourceAssetPath) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source texture path must be provided.", nameof(sourceAssetPath));
        }

        if (TryReadTextureAsset(sourceAssetPath, out TextureAsset textureAsset)) {
            return textureAsset;
        }

        return SourceTextureDecoder.Decode(ResolveBitmapSourcePath(sourceAssetPath));
    }

    /// <summary>
    /// Attempts to read one serialized texture asset from the supplied source path.
    /// </summary>
    /// <param name="sourcePath">Absolute source path that may point at one serialized texture asset.</param>
    /// <param name="textureAsset">Resolved texture asset when one serialized asset was read successfully.</param>
    /// <returns>True when the supplied path contains one serialized texture asset; otherwise false.</returns>
    static bool TryReadTextureAsset(string sourcePath, out TextureAsset textureAsset) {
        textureAsset = null;
        if (string.IsNullOrWhiteSpace(sourcePath) || !File.Exists(sourcePath)) {
            return false;
        }

        string extension = Path.GetExtension(sourcePath);
        if (!string.Equals(extension, ".hasset", StringComparison.OrdinalIgnoreCase)
            && !string.Equals(extension, ".hetex", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        using FileStream stream = new(sourcePath, FileMode.Open, FileAccess.Read, FileShare.Read);
        EngineBinaryHeader header;
        try {
            header = FilesEngineBinaryHeaderSerializer.Read(stream);
        } catch (InvalidOperationException) {
            return false;
        }

        if (header.RecordKind != (ushort)EditorBinaryRecordKind.Asset) {
            return false;
        }

        stream.Position = 0;
        textureAsset = FilesAssetSerializer.Deserialize(stream) as TextureAsset;
        return textureAsset != null;
    }

    /// <summary>
    /// Resolves the bitmap source path that should be imported when one source path is not a serialized texture asset.
    /// </summary>
    /// <param name="sourcePath">Absolute source path emitted by the editor work item.</param>
    /// <returns>Absolute bitmap source path that System.Drawing can load.</returns>
    static string ResolveBitmapSourcePath(string sourcePath) {
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        }

        if (TryResolveImportSettingsSiblingSourcePath(sourcePath, out string siblingSourcePath)) {
            return siblingSourcePath;
        }

        return sourcePath;
    }

    /// <summary>
    /// Attempts to resolve the sibling authored image path for one import-settings sidecar.
    /// </summary>
    /// <param name="sourcePath">Absolute work-item source path that may point at one `.hasset` import-settings sidecar.</param>
    /// <param name="siblingSourcePath">Resolved authored image path when one matching sibling exists.</param>
    /// <returns>True when one sibling source image was found beside the import-settings file; otherwise false.</returns>
    static bool TryResolveImportSettingsSiblingSourcePath(string sourcePath, out string siblingSourcePath) {
        siblingSourcePath = string.Empty;
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        }

        if (!string.Equals(Path.GetExtension(sourcePath), ".hasset", StringComparison.OrdinalIgnoreCase)) {
            return false;
        }

        string directoryPath = Path.GetDirectoryName(sourcePath) ?? string.Empty;
        if (string.IsNullOrWhiteSpace(directoryPath) || !Directory.Exists(directoryPath)) {
            return false;
        }

        string fileNameWithoutExtension = Path.GetFileNameWithoutExtension(sourcePath);
        string[] candidateExtensions = [".png", ".tga", ".jpg", ".jpeg", ".bmp", ".t3x"];
        for (int index = 0; index < candidateExtensions.Length; index++) {
            string candidatePath = Path.Combine(directoryPath, fileNameWithoutExtension + candidateExtensions[index]);
            if (!File.Exists(candidatePath)) {
                continue;
            }

            siblingSourcePath = candidatePath;
            return true;
        }

        return false;
    }

}
