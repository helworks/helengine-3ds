namespace helengine.nintendo3ds.builder.tests.Builders;

/// <summary>
/// Captures the Nintendo 3DS build workspace and emits a stub package file for orchestration tests.
/// </summary>
public sealed class FakeNintendo3DsNativeBuildExecutor : INintendo3DsNativeBuildExecutor {
    /// <summary>
    /// Gets the last workspace passed to the fake executor.
    /// </summary>
    public Nintendo3DsBuildWorkspace Workspace { get; private set; }

    /// <summary>
    /// Captures the workspace and writes a stub exported package file.
    /// </summary>
    /// <param name="workspace">Resolved build workspace under test.</param>
    /// <param name="cancellationToken">Cancellation token forwarded by the caller.</param>
    public void Build(Nintendo3DsBuildWorkspace workspace, CancellationToken cancellationToken) {
        Workspace = workspace ?? throw new ArgumentNullException(nameof(workspace));
        string exportPackageDirectoryPath = Path.GetDirectoryName(workspace.ExportPackagePath)
            ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS export package directory.");
        Directory.CreateDirectory(exportPackageDirectoryPath);
        File.WriteAllText(workspace.ExportPackagePath, "3dsx");
    }
}
