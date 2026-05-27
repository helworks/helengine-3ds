namespace helengine.nintendo3ds.builder;

/// <summary>
/// Represents the resolved file-system workspace for one Nintendo 3DS build.
/// </summary>
public sealed class Nintendo3DsBuildWorkspace {
    /// <summary>
    /// Initializes one Nintendo 3DS build workspace with explicit resolved paths.
    /// </summary>
    /// <param name="repositoryRootPath">Nintendo 3DS repository root that owns the native build.</param>
    /// <param name="workingRootPath">Working root used for staged build inputs.</param>
    /// <param name="outputRootPath">Output root that receives the exported package.</param>
    /// <param name="generatedCoreRootPath">Generated core C++ root prepared by the editor.</param>
    /// <param name="romFsRootPath">Staged RomFS root on the host filesystem.</param>
    /// <param name="containerGeneratedCoreRootPath">Generated-core root path seen by the Docker container.</param>
    /// <param name="containerRomFsRootPath">RomFS root path seen by the Docker container.</param>
    /// <param name="repositoryPackagePath">Package path emitted by the repository-native build.</param>
    /// <param name="exportPackagePath">Final exported package path returned to the caller.</param>
    Nintendo3DsBuildWorkspace(
        string repositoryRootPath,
        string workingRootPath,
        string outputRootPath,
        string generatedCoreRootPath,
        string romFsRootPath,
        string containerGeneratedCoreRootPath,
        string containerRomFsRootPath,
        string repositoryPackagePath,
        string exportPackagePath) {
        RepositoryRootPath = repositoryRootPath;
        WorkingRootPath = workingRootPath;
        OutputRootPath = outputRootPath;
        GeneratedCoreRootPath = generatedCoreRootPath;
        RomFsRootPath = romFsRootPath;
        ContainerGeneratedCoreRootPath = containerGeneratedCoreRootPath;
        ContainerRomFsRootPath = containerRomFsRootPath;
        RepositoryPackagePath = repositoryPackagePath;
        ExportPackagePath = exportPackagePath;
    }

    /// <summary>
    /// Gets the Nintendo 3DS repository root that owns the native build inputs.
    /// </summary>
    public string RepositoryRootPath { get; }

    /// <summary>
    /// Gets the working root used for staged build inputs.
    /// </summary>
    public string WorkingRootPath { get; }

    /// <summary>
    /// Gets the output root that receives the exported package.
    /// </summary>
    public string OutputRootPath { get; }

    /// <summary>
    /// Gets the generated core C++ root prepared by the editor.
    /// </summary>
    public string GeneratedCoreRootPath { get; }

    /// <summary>
    /// Gets the staged RomFS root on the host filesystem.
    /// </summary>
    public string RomFsRootPath { get; }

    /// <summary>
    /// Gets the generated-core root path as seen by the Docker container.
    /// </summary>
    public string ContainerGeneratedCoreRootPath { get; }

    /// <summary>
    /// Gets the RomFS root path as seen by the Docker container.
    /// </summary>
    public string ContainerRomFsRootPath { get; }

    /// <summary>
    /// Gets the repository-local package path emitted by the native build.
    /// </summary>
    public string RepositoryPackagePath { get; }

    /// <summary>
    /// Gets the final exported package path returned to the caller.
    /// </summary>
    public string ExportPackagePath { get; }

    /// <summary>
    /// Creates one Nintendo 3DS build workspace from the caller-supplied repository, working, and output roots.
    /// </summary>
    /// <param name="repositoryRootPath">Nintendo 3DS repository root that owns the native build.</param>
    /// <param name="workingRootPath">Working root used for staged build inputs.</param>
    /// <param name="outputRootPath">Output root that receives the exported package.</param>
    /// <param name="generatedCoreRootPath">Generated core C++ root prepared by the editor.</param>
    /// <returns>Resolved Nintendo 3DS build workspace.</returns>
    public static Nintendo3DsBuildWorkspace Create(
        string repositoryRootPath,
        string workingRootPath,
        string outputRootPath,
        string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(repositoryRootPath)) {
            throw new ArgumentException("Repository root path must be provided.", nameof(repositoryRootPath));
        } else if (string.IsNullOrWhiteSpace(workingRootPath)) {
            throw new ArgumentException("Working root path must be provided.", nameof(workingRootPath));
        } else if (string.IsNullOrWhiteSpace(outputRootPath)) {
            throw new ArgumentException("Output root path must be provided.", nameof(outputRootPath));
        } else if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }

        string fullRepositoryRootPath = Path.GetFullPath(repositoryRootPath);
        string fullWorkingRootPath = Path.GetFullPath(workingRootPath);
        string fullOutputRootPath = Path.GetFullPath(outputRootPath);
        string fullGeneratedCoreRootPath = Path.GetFullPath(generatedCoreRootPath);
        string romFsRootPath = Path.Combine(fullWorkingRootPath, "3ds", "romfs");
        string repositoryPackagePath = Path.Combine(fullRepositoryRootPath, "build", "helengine_3ds.3dsx");
        string exportPackagePath = Path.Combine(fullOutputRootPath, "helengine_3ds.3dsx");

        return new Nintendo3DsBuildWorkspace(
            fullRepositoryRootPath,
            fullWorkingRootPath,
            fullOutputRootPath,
            fullGeneratedCoreRootPath,
            romFsRootPath,
            "/workspace-generated-core",
            "/workspace-staging/3ds/romfs",
            repositoryPackagePath,
            exportPackagePath);
    }
}
