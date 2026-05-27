namespace helengine.nintendo3ds.builder;

/// <summary>
/// Stores the Nintendo 3DS-owned startup-scene identifiers used by the generated boot-scene startup contract.
/// </summary>
public static class Nintendo3DsStartupSceneIds {
    /// <summary>
    /// Scene id of the generated boot scene that Nintendo 3DS must use as its effective startup scene.
    /// </summary>
    public const string GeneratedBootSceneId = "GeneratedBootScene";

    /// <summary>
    /// Cooked runtime-relative payload path of the generated boot scene.
    /// </summary>
    public const string GeneratedBootSceneCookedRelativePath = "cooked/scenes/generatedbootscene.hasset";
}
