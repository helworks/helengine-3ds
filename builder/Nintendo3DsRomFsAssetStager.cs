using helengine.baseplatform.Manifest;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Copies cooked runtime payloads from the package root into the staged Nintendo 3DS RomFS tree.
/// </summary>
public sealed class Nintendo3DsRomFsAssetStager {
    /// <summary>
    /// Stable scene id used by the generated boot-scene startup contract.
    /// </summary>
    const string GeneratedBootSceneId = "GeneratedBootScene";

    /// <summary>
    /// Stable canonical RomFS path used to boot the generated startup scene on Nintendo 3DS.
    /// </summary>
    const string GeneratedBootSceneAliasRelativePath = "cooked/scenes/generatedbootscene.hasset";

    /// <summary>
    /// Stages the cooked payload files referenced by the build manifest into RomFS.
    /// </summary>
    /// <param name="manifest">Build manifest that identifies the cooked payload files to stage.</param>
    /// <param name="sourceRootPath">Package root that already contains the cooked payload files.</param>
    /// <param name="romFsRootPath">RomFS root that should receive the cooked payload files.</param>
    public void Stage(PlatformBuildManifest manifest, string sourceRootPath, string romFsRootPath) {
        if (manifest == null) {
            throw new ArgumentNullException(nameof(manifest));
        } else if (string.IsNullOrWhiteSpace(sourceRootPath)) {
            throw new ArgumentException("RomFS source root path must be provided.", nameof(sourceRootPath));
        } else if (string.IsNullOrWhiteSpace(romFsRootPath)) {
            throw new ArgumentException("RomFS destination root path must be provided.", nameof(romFsRootPath));
        }

        string fullSourceRootPath = Path.GetFullPath(sourceRootPath);
        string fullRomFsRootPath = Path.GetFullPath(romFsRootPath);
        string sourceRootPrefix = EnsureTrailingDirectorySeparator(fullSourceRootPath);
        HashSet<string> stagedRelativePaths = new(StringComparer.OrdinalIgnoreCase);

        Directory.CreateDirectory(fullRomFsRootPath);

        for (int index = 0; index < manifest.Scenes.Length; index++) {
            PlatformBuildPayloadReference[] payloadReferences = manifest.Scenes[index].PayloadReferences;
            for (int payloadIndex = 0; payloadIndex < payloadReferences.Length; payloadIndex++) {
                StagePayloadReference(payloadReferences[payloadIndex], sourceRootPrefix, fullRomFsRootPath, stagedRelativePaths);
            }

            StageGeneratedBootSceneAlias(manifest.Scenes[index], sourceRootPrefix, fullRomFsRootPath, stagedRelativePaths);
        }

        for (int index = 0; index < manifest.LooseAssets.Length; index++) {
            StagePayloadReference(manifest.LooseAssets[index].PayloadReference, sourceRootPrefix, fullRomFsRootPath, stagedRelativePaths);
        }

        PlatformCookWorkItem[] platformCookWorkItems = manifest.PlatformCookWorkItems ?? [];
        for (int index = 0; index < platformCookWorkItems.Length; index++) {
            StageCookWorkItemOutput(platformCookWorkItems[index], sourceRootPrefix, fullRomFsRootPath, stagedRelativePaths);
        }
    }

    /// <summary>
    /// Stages one payload reference into RomFS when it has not already been copied.
    /// </summary>
    /// <param name="payloadReference">Payload reference that identifies one cooked file.</param>
    /// <param name="sourceRootPrefix">Normalized source-root path with a trailing directory separator.</param>
    /// <param name="romFsRootPath">RomFS root that receives the copied file.</param>
    /// <param name="stagedRelativePaths">Set of already staged relative paths.</param>
    static void StagePayloadReference(
        PlatformBuildPayloadReference payloadReference,
        string sourceRootPrefix,
        string romFsRootPath,
        HashSet<string> stagedRelativePaths) {
        if (payloadReference == null) {
            throw new ArgumentNullException(nameof(payloadReference));
        } else if (stagedRelativePaths == null) {
            throw new ArgumentNullException(nameof(stagedRelativePaths));
        }

        string relativePath = NormalizeRelativePath(payloadReference.SourceIdentity);
        if (!stagedRelativePaths.Add(relativePath)) {
            return;
        }

        string sourceRootPath = sourceRootPrefix.TrimEnd(Path.DirectorySeparatorChar);
        string sourceFilePath = Path.GetFullPath(Path.Combine(sourceRootPath, relativePath.Replace('/', Path.DirectorySeparatorChar)));
        if (!sourceFilePath.StartsWith(sourceRootPrefix, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException("Nintendo 3DS payload paths must stay inside the package root.");
        } else if (!File.Exists(sourceFilePath)) {
            throw new InvalidOperationException($"Nintendo 3DS payload '{relativePath}' was not found in the package root.");
        }

        string destinationFilePath = Path.Combine(romFsRootPath, relativePath.Replace('/', Path.DirectorySeparatorChar));
        string destinationDirectoryPath = Path.GetDirectoryName(destinationFilePath)
            ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS RomFS destination directory.");
        Directory.CreateDirectory(destinationDirectoryPath);
        File.Copy(sourceFilePath, destinationFilePath, overwrite: true);
    }

    /// <summary>
    /// Stages the stable generated-boot-scene alias required by Nintendo 3DS startup when the generated boot scene uses a hashed cooked payload path.
    /// </summary>
    /// <param name="scene">Scene whose payloads may require the stable startup-scene alias.</param>
    /// <param name="sourceRootPrefix">Normalized source-root path with a trailing directory separator.</param>
    /// <param name="romFsRootPath">RomFS root that receives the copied file.</param>
    /// <param name="stagedRelativePaths">Set of already staged relative paths.</param>
    static void StageGeneratedBootSceneAlias(
        PlatformBuildScene scene,
        string sourceRootPrefix,
        string romFsRootPath,
        HashSet<string> stagedRelativePaths) {
        if (scene == null) {
            throw new ArgumentNullException(nameof(scene));
        } else if (!string.Equals(scene.SceneId, GeneratedBootSceneId, StringComparison.Ordinal)) {
            return;
        }

        StageAliasedRelativePathCopy(
            ResolveGeneratedBootSceneAliasSourceRelativePath(scene),
            GeneratedBootSceneAliasRelativePath,
            sourceRootPrefix,
            romFsRootPath,
            stagedRelativePaths);
    }

    /// <summary>
    /// Resolves the cooked scene payload path that should back the stable generated-boot-scene RomFS alias.
    /// </summary>
    /// <param name="scene">Generated boot-scene manifest entry whose cooked path should be inspected.</param>
    /// <returns>Canonical cooked-relative scene payload path.</returns>
    static string ResolveGeneratedBootSceneAliasSourceRelativePath(PlatformBuildScene scene) {
        if (scene == null) {
            throw new ArgumentNullException(nameof(scene));
        } else if (scene.ResolvedMetadata == null) {
            throw new InvalidOperationException("Nintendo 3DS generated boot scene must define resolved metadata.");
        }

        for (int index = 0; index < scene.ResolvedMetadata.Length; index++) {
            KeyValuePair<string, string> metadata = scene.ResolvedMetadata[index];
            if (!string.Equals(metadata.Key, PlatformBuildSceneMetadataKeys.CookedRelativePath, StringComparison.OrdinalIgnoreCase)) {
                continue;
            } else if (string.IsNullOrWhiteSpace(metadata.Value)) {
                throw new InvalidOperationException("Nintendo 3DS generated boot scene cooked-relative-path metadata must not be empty.");
            }

            return metadata.Value;
        }

        throw new InvalidOperationException("Nintendo 3DS generated boot scene must define cooked-relative-path metadata.");
    }

    /// <summary>
    /// Copies one source-relative payload into one explicit RomFS alias path when that alias has not already been staged.
    /// </summary>
    /// <param name="sourceRelativePath">Canonical source-relative payload path inside the package root.</param>
    /// <param name="destinationRelativePath">Canonical RomFS-relative alias path.</param>
    /// <param name="sourceRootPrefix">Normalized source-root path with a trailing directory separator.</param>
    /// <param name="romFsRootPath">RomFS root that receives the copied file.</param>
    /// <param name="stagedRelativePaths">Set of already staged relative paths.</param>
    static void StageAliasedRelativePathCopy(
        string sourceRelativePath,
        string destinationRelativePath,
        string sourceRootPrefix,
        string romFsRootPath,
        HashSet<string> stagedRelativePaths) {
        if (string.IsNullOrWhiteSpace(sourceRelativePath)) {
            throw new InvalidOperationException("Nintendo 3DS aliased payload source path must be provided.");
        } else if (string.IsNullOrWhiteSpace(destinationRelativePath)) {
            throw new InvalidOperationException("Nintendo 3DS aliased payload destination path must be provided.");
        } else if (stagedRelativePaths == null) {
            throw new ArgumentNullException(nameof(stagedRelativePaths));
        }

        string normalizedSourceRelativePath = NormalizeRelativePath(sourceRelativePath);
        string normalizedDestinationRelativePath = NormalizeRelativePath(destinationRelativePath);
        if (!stagedRelativePaths.Add(normalizedDestinationRelativePath)) {
            return;
        }

        string sourceRootPath = sourceRootPrefix.TrimEnd(Path.DirectorySeparatorChar);
        string sourceFilePath = Path.GetFullPath(Path.Combine(sourceRootPath, normalizedSourceRelativePath.Replace('/', Path.DirectorySeparatorChar)));
        if (!sourceFilePath.StartsWith(sourceRootPrefix, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException("Nintendo 3DS aliased payload paths must stay inside the package root.");
        } else if (!File.Exists(sourceFilePath)) {
            throw new InvalidOperationException($"Nintendo 3DS aliased payload source '{normalizedSourceRelativePath}' was not found in the package root.");
        }

        string destinationFilePath = Path.Combine(romFsRootPath, normalizedDestinationRelativePath.Replace('/', Path.DirectorySeparatorChar));
        string destinationDirectoryPath = Path.GetDirectoryName(destinationFilePath)
            ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS aliased RomFS destination directory.");
        Directory.CreateDirectory(destinationDirectoryPath);
        File.Copy(sourceFilePath, destinationFilePath, overwrite: true);
    }

    /// <summary>
    /// Stages one builder-owned platform cook work-item output into RomFS when it has not already been copied.
    /// </summary>
    /// <param name="workItem">Builder-owned platform cook work item whose output should be staged.</param>
    /// <param name="sourceRootPrefix">Normalized source-root path with a trailing directory separator.</param>
    /// <param name="romFsRootPath">RomFS root that receives the copied file.</param>
    /// <param name="stagedRelativePaths">Set of already staged relative paths.</param>
    static void StageCookWorkItemOutput(
        PlatformCookWorkItem workItem,
        string sourceRootPrefix,
        string romFsRootPath,
        HashSet<string> stagedRelativePaths) {
        if (workItem == null) {
            throw new ArgumentNullException(nameof(workItem));
        }

        StageRelativePath(workItem.OutputRelativePath, sourceRootPrefix, romFsRootPath, stagedRelativePaths);
    }

    /// <summary>
    /// Stages one relative output path into RomFS when it has not already been copied.
    /// </summary>
    /// <param name="relativePath">Runtime-relative path that should be copied from the package source root.</param>
    /// <param name="sourceRootPrefix">Normalized source-root path with a trailing directory separator.</param>
    /// <param name="romFsRootPath">RomFS root that receives the copied file.</param>
    /// <param name="stagedRelativePaths">Set of already staged relative paths.</param>
    static void StageRelativePath(
        string relativePath,
        string sourceRootPrefix,
        string romFsRootPath,
        HashSet<string> stagedRelativePaths) {
        if (stagedRelativePaths == null) {
            throw new ArgumentNullException(nameof(stagedRelativePaths));
        }

        string normalizedRelativePath = NormalizeRelativePath(relativePath);
        if (!stagedRelativePaths.Add(normalizedRelativePath)) {
            return;
        }

        string sourceRootPath = sourceRootPrefix.TrimEnd(Path.DirectorySeparatorChar);
        string sourceFilePath = Path.GetFullPath(Path.Combine(sourceRootPath, normalizedRelativePath.Replace('/', Path.DirectorySeparatorChar)));
        if (!sourceFilePath.StartsWith(sourceRootPrefix, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException("Nintendo 3DS payload paths must stay inside the package root.");
        } else if (!File.Exists(sourceFilePath)) {
            throw new InvalidOperationException($"Nintendo 3DS payload '{normalizedRelativePath}' was not found in the package root.");
        }

        string destinationFilePath = Path.Combine(romFsRootPath, normalizedRelativePath.Replace('/', Path.DirectorySeparatorChar));
        string destinationDirectoryPath = Path.GetDirectoryName(destinationFilePath)
            ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS RomFS destination directory.");
        Directory.CreateDirectory(destinationDirectoryPath);
        File.Copy(sourceFilePath, destinationFilePath, overwrite: true);
    }

    /// <summary>
    /// Normalizes one payload path to a forward-slash relative path.
    /// </summary>
    /// <param name="path">Payload path to normalize.</param>
    /// <returns>Normalized forward-slash relative path.</returns>
    static string NormalizeRelativePath(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            throw new InvalidOperationException("Nintendo 3DS payload paths must be provided.");
        }

        string normalizedPath = path.Replace('\\', '/');
        if (Path.IsPathRooted(normalizedPath)) {
            throw new InvalidOperationException("Nintendo 3DS payload paths must not be rooted.");
        } else if (normalizedPath.StartsWith("../", StringComparison.Ordinal) || normalizedPath.Contains("/../", StringComparison.Ordinal)) {
            throw new InvalidOperationException("Nintendo 3DS payload paths must stay inside the package root.");
        }

        return normalizedPath;
    }

    /// <summary>
    /// Ensures a path ends with one directory separator.
    /// </summary>
    /// <param name="path">Path to normalize.</param>
    /// <returns>Path with a trailing directory separator.</returns>
    static string EnsureTrailingDirectorySeparator(string path) {
        if (string.IsNullOrWhiteSpace(path)) {
            throw new ArgumentException("Path must be provided.", nameof(path));
        }

        return path.EndsWith(Path.DirectorySeparatorChar)
            ? path
            : path + Path.DirectorySeparatorChar;
    }
}
