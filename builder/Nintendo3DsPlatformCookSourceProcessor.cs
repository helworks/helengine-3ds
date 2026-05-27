using System.Runtime.Versioning;

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

        TextureAsset decodedTextureAsset = SourceTextureDecoder.Decode(sourceAssetPath);
        TextureAsset cookedTextureAsset = ApplySettings(decodedTextureAsset, settings);
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
            sourceTextureAsset = SourceFontAtlasTextureImporter.Import(sourceAssetPath);
        }

        TextureAsset cookedTextureAsset = ApplySettings(sourceTextureAsset, settings);
        cookedTextureAsset.Id = assetId;
        cookedTextureAsset.RuntimeAssetId = RuntimeAssetIdGenerator.Generate(assetId);
        return cookedTextureAsset;
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
        FontAsset fontAsset = global::helengine.files.FontAssetBinarySerializer.Deserialize(stream);
        return fontAsset.SourceTextureAsset;
    }

    /// <summary>
    /// Applies the Nintendo 3DS shared generic texture settings to one decoded texture asset.
    /// </summary>
    /// <param name="asset">Decoded texture asset to process.</param>
    /// <param name="settings">Resolved 3DS texture processor settings.</param>
    /// <returns>Processed texture asset.</returns>
    static TextureAsset ApplySettings(TextureAsset asset, TextureAssetProcessorSettings settings) {
        if (asset == null) {
            throw new ArgumentNullException(nameof(asset));
        } else if (settings == null) {
            throw new ArgumentNullException(nameof(settings));
        } else if (asset.Width < 1 || asset.Height < 1) {
            throw new InvalidOperationException("Texture assets must have positive dimensions.");
        } else if (asset.Colors == null) {
            throw new InvalidOperationException("Texture assets must include color data.");
        } else if (settings.MaxResolution < 0) {
            throw new InvalidOperationException("Texture max resolution cannot be negative.");
        } else if (!string.Equals(settings.ColorFormatId, TextureAssetColorFormat.Rgba32.ToString(), StringComparison.Ordinal)) {
            throw new InvalidOperationException($"Nintendo 3DS texture color format '{settings.ColorFormatId}' is not supported by the current builder-owned cook path.");
        } else if (settings.AlphaPrecision != TextureAssetAlphaPrecision.A8) {
            throw new InvalidOperationException($"Nintendo 3DS texture alpha precision '{settings.AlphaPrecision}' is not supported by the current builder-owned cook path.");
        }

        if (settings.MaxResolution > 0 && (asset.Width > settings.MaxResolution || asset.Height > settings.MaxResolution)) {
            return ResizeToMaxResolution(asset, settings.MaxResolution);
        }

        return asset;
    }

    /// <summary>
    /// Builds one resized texture asset whose larger axis matches the supplied cap.
    /// </summary>
    /// <param name="asset">Texture asset to resize.</param>
    /// <param name="maxResolution">Maximum allowed width or height.</param>
    /// <returns>Resized texture asset.</returns>
    static TextureAsset ResizeToMaxResolution(TextureAsset asset, int maxResolution) {
        double largestDimension = Math.Max(asset.Width, asset.Height);
        double scale = maxResolution / largestDimension;
        int resizedWidth = Math.Max(1, (int)Math.Round(asset.Width * scale));
        int resizedHeight = Math.Max(1, (int)Math.Round(asset.Height * scale));
        byte[] resizedColors = new byte[resizedWidth * resizedHeight * 4];

        for (int y = 0; y < resizedHeight; y++) {
            int sourceY = GetSourceCoordinate(y, resizedHeight, asset.Height);
            for (int x = 0; x < resizedWidth; x++) {
                int sourceX = GetSourceCoordinate(x, resizedWidth, asset.Width);
                int sourceIndex = ((sourceY * asset.Width) + sourceX) * 4;
                int targetIndex = ((y * resizedWidth) + x) * 4;
                Buffer.BlockCopy(asset.Colors, sourceIndex, resizedColors, targetIndex, 4);
            }
        }

        return new TextureAsset {
            Id = asset.Id,
            RuntimeAssetId = asset.RuntimeAssetId,
            Width = (ushort)resizedWidth,
            Height = (ushort)resizedHeight,
            ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = TextureAssetAlphaPrecision.A8,
            Colors = resizedColors,
            PaletteColors = Array.Empty<byte>()
        };
    }

    /// <summary>
    /// Resolves one source-axis coordinate for nearest-neighbor resizing.
    /// </summary>
    /// <param name="targetCoordinate">Zero-based coordinate in the resized dimension.</param>
    /// <param name="targetSize">Size of the resized dimension.</param>
    /// <param name="sourceSize">Size of the source dimension.</param>
    /// <returns>Nearest source coordinate.</returns>
    static int GetSourceCoordinate(int targetCoordinate, int targetSize, int sourceSize) {
        if (targetSize <= 1 || sourceSize <= 1) {
            return 0;
        }

        double ratio = targetCoordinate / (double)(targetSize - 1);
        return Math.Clamp((int)Math.Round(ratio * (sourceSize - 1)), 0, sourceSize - 1);
    }
}
