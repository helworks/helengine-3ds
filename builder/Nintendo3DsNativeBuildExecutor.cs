using helengine.baseplatform.Builders;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Provides the default native Nintendo 3DS build executor for the RomFS-backed startup-manifest slice.
/// </summary>
public sealed class Nintendo3DsNativeBuildExecutor : INintendo3DsNativeBuildExecutor {
    /// <summary>
    /// Runs one Docker-backed Nintendo 3DS packaging build and exports the resulting package.
    /// </summary>
    /// <param name="workspace">Resolved build workspace to package.</param>
    /// <param name="cancellationToken">Cancellation token used to stop the build cooperatively.</param>
    public void Build(Nintendo3DsBuildWorkspace workspace, CancellationToken cancellationToken) {
        if (workspace == null) {
            throw new ArgumentNullException(nameof(workspace));
        }

        Directory.CreateDirectory(workspace.OutputRootPath);
        Directory.CreateDirectory(workspace.RomFsRootPath);

        RunDockerMake(workspace, cancellationToken, "clean");
        RunDockerMake(
            workspace,
            cancellationToken,
            "HELENGINE_3DS_ROMFS_ROOT=" + workspace.ContainerRomFsRootPath,
            "HELENGINE_CORE_CPP_ROOT=" + workspace.ContainerGeneratedCoreRootPath);

        if (!File.Exists(workspace.RepositoryPackagePath)) {
            throw new InvalidOperationException("Nintendo 3DS package output was not produced.");
        }

        string exportPackageDirectoryPath = Path.GetDirectoryName(workspace.ExportPackagePath)
            ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS export package directory.");
        Directory.CreateDirectory(exportPackageDirectoryPath);
        File.Copy(workspace.RepositoryPackagePath, workspace.ExportPackagePath, true);
    }

    /// <summary>
    /// Runs one Docker-backed <c>make</c> command inside the Nintendo 3DS repository workspace.
    /// </summary>
    /// <param name="workspace">Resolved build workspace used for repository and staging mounts.</param>
    /// <param name="cancellationToken">Cancellation token used to stop the build cooperatively.</param>
    /// <param name="makeArguments">Arguments forwarded to <c>make</c>.</param>
    static void RunDockerMake(Nintendo3DsBuildWorkspace workspace, CancellationToken cancellationToken, params string[] makeArguments) {
        System.Diagnostics.ProcessStartInfo startInfo = new() {
            FileName = "docker",
            WorkingDirectory = workspace.RepositoryRootPath,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };
        startInfo.ArgumentList.Add("run");
        startInfo.ArgumentList.Add("--rm");
        startInfo.ArgumentList.Add("-v");
        startInfo.ArgumentList.Add(workspace.RepositoryRootPath + ":/workspace");
        startInfo.ArgumentList.Add("-v");
        startInfo.ArgumentList.Add(workspace.WorkingRootPath + ":/workspace-staging");
        startInfo.ArgumentList.Add("-v");
        startInfo.ArgumentList.Add(workspace.GeneratedCoreRootPath + ":" + workspace.ContainerGeneratedCoreRootPath);
        startInfo.ArgumentList.Add("-w");
        startInfo.ArgumentList.Add("/workspace");
        startInfo.ArgumentList.Add("helengine-3ds");
        startInfo.ArgumentList.Add("make");
        for (int index = 0; index < makeArguments.Length; index++) {
            if (!string.IsNullOrWhiteSpace(makeArguments[index])) {
                startInfo.ArgumentList.Add(makeArguments[index]);
            }
        }

        NativeProcessRunResult result = new NativeProcessRunner().Run(startInfo, cancellationToken);
        if (result.ExitCode != 0) {
            throw new InvalidOperationException(
                "Nintendo 3DS Docker build failed." + Environment.NewLine +
                "Repository root: " + workspace.RepositoryRootPath + Environment.NewLine +
                "Working root: " + workspace.WorkingRootPath + Environment.NewLine +
                "Repository Makefile present: " + File.Exists(Path.Combine(workspace.RepositoryRootPath, "Makefile")) + Environment.NewLine +
                "Arguments: make " + string.Join(" ", makeArguments.Where(argument => !string.IsNullOrWhiteSpace(argument))) + Environment.NewLine +
                result.StandardOutput + Environment.NewLine + result.StandardError);
        }
    }
}
