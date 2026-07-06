namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Verifies the resolved filesystem layout used for one Nintendo 3DS build.
/// </summary>
public class Nintendo3DsBuildWorkspaceTests {
    /// <summary>
    /// Verifies the workspace builds the expected RomFS staging and package-export paths.
    /// </summary>
    [Fact]
    public void Create_builds_expected_staging_and_output_paths() {
        string repositoryRoot = "/mnt/c/dev/helworks/helengine-3ds";
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-work-" + Guid.NewGuid().ToString("N"));
        string outputRoot = Path.Combine(workingRoot, "out");
        string generatedCoreRoot = Path.Combine(workingRoot, "generated-core");

        Nintendo3DsBuildWorkspace workspace = Nintendo3DsBuildWorkspace.Create(
            repositoryRoot,
            workingRoot,
            outputRoot,
            generatedCoreRoot);

        Assert.Equal(Path.GetFullPath(repositoryRoot), workspace.RepositoryRootPath);
        Assert.Equal(Path.GetFullPath(generatedCoreRoot), workspace.GeneratedCoreRootPath);
        Assert.Equal(Path.GetFullPath(Path.Combine(workingRoot, "builder", "3ds", "romfs")), workspace.RomFsRootPath);
        Assert.Equal("/workspace-generated-core", workspace.ContainerGeneratedCoreRootPath);
        Assert.Equal("/workspace-staging/builder/3ds/romfs", workspace.ContainerRomFsRootPath);
        Assert.Equal(Path.GetFullPath(Path.Combine(repositoryRoot, "build", "helengine_3ds.3dsx")), workspace.RepositoryPackagePath);
        Assert.Equal(Path.GetFullPath(Path.Combine(outputRoot, "helengine_3ds.3dsx")), workspace.ExportPackagePath);
    }
}
