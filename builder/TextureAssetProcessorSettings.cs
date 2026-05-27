namespace helengine.nintendo3ds.builder;

/// <summary>
/// Stores one platform-specific texture processor configuration record for a Nintendo 3DS builder-owned source asset.
/// </summary>
public sealed class TextureAssetProcessorSettings {
    /// <summary>
    /// Gets or sets the maximum allowed width or height in pixels for the processed texture, or zero when uncapped.
    /// </summary>
    public int MaxResolution { get; set; }

    /// <summary>
    /// Gets or sets the platform-published texture color-format identifier produced for this platform.
    /// </summary>
    public string ColorFormatId { get; set; } = TextureAssetColorFormat.Rgba32.ToString();

    /// <summary>
    /// Gets or sets the alpha precision stored by the processed texture payload.
    /// </summary>
    public TextureAssetAlphaPrecision AlphaPrecision { get; set; }
}
