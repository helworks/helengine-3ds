namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Audits the Nintendo 3DS native executor so Docker output remains visible while the native build is running.
/// </summary>
public sealed class Nintendo3DsNativeBuildExecutorSourceAuditTests {
    /// <summary>
    /// Verifies the executor delegates Docker output and process completion to the shared deterministic runner.
    /// </summary>
    [Fact]
    public void Source_whenRunningDockerBuild_streamsBothOutputChannelsBeforeProcessExit() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "builder", "Nintendo3DsNativeBuildExecutor.cs");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("new NativeProcessRunner().Run(startInfo, cancellationToken)", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("ReadToEndAsync", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("WaitForExit(100)", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("Task.WaitAll", sourceCode, StringComparison.Ordinal);
    }
}
