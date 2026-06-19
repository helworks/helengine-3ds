namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Guards the canonical Nintendo 3DS emulator launcher contract.
/// </summary>
public sealed class Nintendo3DsLauncherScriptTests {
    /// <summary>
    /// Ensures the canonical launcher requires one explicit artifact path and starts the configured Nintendo 3DS emulator with a .3dsx package.
    /// </summary>
    [Fact]
    public void Launcher_RequiresArtifactPath_AndLaunchesNintendo3DsEmulator() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string scriptPath = Path.Combine(repositoryRootPath, "scripts", "launch_in_emulator.ps1");

        Assert.True(File.Exists(scriptPath), "Expected scripts/launch_in_emulator.ps1 to exist.");

        string scriptSource = File.ReadAllText(scriptPath);

        Assert.Contains("[string]$ArtifactPath", scriptSource, StringComparison.Ordinal);
        Assert.Contains(".3dsx", scriptSource, StringComparison.Ordinal);
        Assert.Contains("Lime3DS", scriptSource, StringComparison.Ordinal);
    }
}
