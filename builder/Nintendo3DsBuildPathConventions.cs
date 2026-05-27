namespace helengine.nintendo3ds.builder;

/// <summary>
/// Defines the builder-owned working-root path conventions used by Nintendo 3DS builds.
/// </summary>
internal static class Nintendo3DsBuildPathConventions {
    /// <summary>
    /// Stores the working-root child directory that contains the staged package source consumed by the builder.
    /// </summary>
    public const string PackageSourceDirectoryName = "package-source";

    /// <summary>
    /// Resolves the builder-owned staged package source root from one Nintendo 3DS working root.
    /// </summary>
    /// <param name="workingRootPath">Working root used for the Nintendo 3DS build.</param>
    /// <returns>Absolute staged package source root path.</returns>
    public static string ResolvePackageSourceRootPath(string workingRootPath) {
        if (string.IsNullOrWhiteSpace(workingRootPath)) {
            throw new ArgumentException("Working root path must be provided.", nameof(workingRootPath));
        }

        return Path.Combine(Path.GetFullPath(workingRootPath), PackageSourceDirectoryName);
    }
}
