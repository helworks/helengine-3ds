using helengine.baseplatform.Definitions;

namespace helengine.nintendo3ds.builder;

/// <summary>Publishes every PICA encoding alongside the existing RGBA32 image and font defaults.</summary>
public static class Nintendo3DsTextureFormatCapabilities {
    /// <summary>Creates exact color/alpha combinations shared by ordinary textures and native font atlases.</summary>
    public static PlatformTextureFormatCapabilityDefinition Create() {
        List<string> formats = new List<string> { "Rgba32" };
        List<PlatformTextureFormatCombinationDefinition> combinations = new List<PlatformTextureFormatCombinationDefinition> {
            new PlatformTextureFormatCombinationDefinition("Rgba32", TextureAssetAlphaPrecision.A8)
        };
        foreach (Nintendo3DsTextureFormat format in Nintendo3DsTextureFormatCatalog.Formats) {
            formats.Add(format.Id);
            combinations.Add(new PlatformTextureFormatCombinationDefinition(format.Id, TextureAssetAlphaPrecision.Opaque));
            if (format.Alpha != TextureAssetAlphaPrecision.Opaque) combinations.Add(new PlatformTextureFormatCombinationDefinition(format.Id, format.Alpha));
        }
        return new PlatformTextureFormatCapabilityDefinition(formats.ToArray(), new[] { TextureAssetAlphaPrecision.Opaque, TextureAssetAlphaPrecision.Binary, TextureAssetAlphaPrecision.A4, TextureAssetAlphaPrecision.A8 }, combinations.ToArray());
    }
}
