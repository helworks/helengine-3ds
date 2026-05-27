namespace helengine.nintendo3ds.builder;

/// <summary>
/// Stores the Nintendo 3DS-owned startup-scene identifiers used by the current direct main-menu boot flow.
/// </summary>
public static class Nintendo3DsStartupSceneIds {
    /// <summary>
    /// Scene id of the console-authored main menu that Nintendo 3DS boots directly during the current startup contract.
    /// </summary>
    public const string DirectStartupSceneId = "DemoDiscMainMenu";

    /// <summary>
    /// Cooked runtime-relative payload path of the console-authored main menu.
    /// </summary>
    public const string DirectStartupSceneCookedRelativePath = "cooked/scenes/DemoDiscMainMenu.hasset";
}
