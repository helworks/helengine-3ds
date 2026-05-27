namespace helengine.nintendo3ds.builder;

/// <summary>
/// Imports builder-owned Nintendo 3DS source assets and converts them into final runtime texture payloads.
/// </summary>
public interface INintendo3DsPlatformCookSourceProcessor {
    /// <summary>
    /// Imports one source texture asset and applies the resolved Nintendo 3DS texture processor settings.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source texture path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked texture should preserve.</param>
    /// <param name="settings">Resolved Nintendo 3DS texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime texture payload ready for serialization.</returns>
    TextureAsset CookTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings);

    /// <summary>
    /// Imports one source font asset and cooks its atlas texture into the final Nintendo 3DS runtime texture payload.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute source font path emitted by the editor build graph.</param>
    /// <param name="assetId">Stable runtime asset identifier the cooked atlas texture should preserve.</param>
    /// <param name="settings">Resolved Nintendo 3DS texture processor settings supplied by the editor.</param>
    /// <returns>Processed runtime atlas texture payload ready for serialization.</returns>
    TextureAsset CookFontAtlasTexture(string sourceAssetPath, string assetId, TextureAssetProcessorSettings settings);
}
