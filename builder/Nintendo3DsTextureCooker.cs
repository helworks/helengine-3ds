namespace helengine.nintendo3ds.builder;

/// <summary>Resizes decoded source pixels and preserves native PICA storage in builder-owned texture and font-atlas payloads.</summary>
public static class Nintendo3DsTextureCooker {
    /// <summary>Creates an independently owned cooked asset with the same identity and selected native encoding.</summary>
    public static TextureAsset Apply(TextureAsset source, TextureAssetProcessorSettings settings) {
        if (source == null) throw new ArgumentNullException(nameof(source));
        ValidateSettings(settings);
        byte[] rgba = TextureAssetPixelCodec.DecodeToRgba32(source);
        int width = source.Width, height = source.Height;
        if (settings.MaxResolution > 0 && Math.Max(width, height) > settings.MaxResolution) {
            double scale = settings.MaxResolution / (double)Math.Max(width, height);
            int resizedWidth = Math.Max(1, (int)Math.Round(width * scale)), resizedHeight = Math.Max(1, (int)Math.Round(height * scale));
            byte[] resized = new byte[resizedWidth * resizedHeight * 4];
            for (int y = 0; y < resizedHeight; y++) {
                int sourceY = resizedHeight <= 1 ? 0 : (int)Math.Round(y * (height - 1) / (double)(resizedHeight - 1));
                for (int x = 0; x < resizedWidth; x++) {
                    int sourceX = resizedWidth <= 1 ? 0 : (int)Math.Round(x * (width - 1) / (double)(resizedWidth - 1));
                    Array.Copy(rgba, (sourceY * width + sourceX) * 4, resized, (y * resizedWidth + x) * 4, 4);
                }
            }
            rgba = resized; width = resizedWidth; height = resizedHeight;
        }
        if (width > 1024 || height > 1024) throw new ArgumentException("Cooked 3DS textures must fit within 1024 by 1024.");
        TextureAsset decoded = new TextureAsset { Id = source.Id, RuntimeAssetId = source.RuntimeAssetId, AuthoringAssetId = source.AuthoringAssetId,
            FormerAuthoringAssetIds = source.FormerAuthoringAssetIds == null ? Array.Empty<string>() : (string[])source.FormerAuthoringAssetIds.Clone(), IsEngineOwned = source.IsEngineOwned,
            Width = (ushort)width, Height = (ushort)height, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8, Colors = rgba };
        if (settings.ColorFormatId == "Rgba32") return decoded;
        using (decoded) return Nintendo3DsTextureCodec.Encode(decoded, settings.ColorFormatId, settings.AlphaPrecision);
    }
    /// <summary>Rejects unpublished encodings, incompatible alpha precision and out-of-range resolution settings.</summary>
    public static void ValidateSettings(TextureAssetProcessorSettings settings) {
        if (settings == null) throw new ArgumentNullException(nameof(settings));
        if (settings.MaxResolution < 0 || settings.MaxResolution > 1024) throw new ArgumentException("3DS maximum resolution must be zero or within 1..1024.");
        if (Nintendo3DsTextureFormatCatalog.TryGetFormat(settings.ColorFormatId, out Nintendo3DsTextureFormat format)) {
            if (!format.SupportsAlpha(settings.AlphaPrecision)) throw new ArgumentException("The chosen 3DS encoding cannot store the requested alpha precision.");
        } else if (settings.ColorFormatId != "Rgba32" || settings.AlphaPrecision != TextureAssetAlphaPrecision.A8) throw new ArgumentException("Unsupported 3DS texture settings.");
    }
}
