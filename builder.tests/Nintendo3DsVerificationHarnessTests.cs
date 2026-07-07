using helengine.nintendo3ds.builder.tests.Builders;

namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Verifies the builder-owned Nintendo 3DS verification harness flow.
/// </summary>
[Collection(ConsoleOutputTestCollection.Name)]
public class Nintendo3DsVerificationHarnessTests {
    /// <summary>
    /// Verifies the smoke harness runs the builder-owned staging flow and exports a package through the native executor seam.
    /// </summary>
    [Fact]
    public void RunSmokeTest_whenUsingFakeExecutor_stages_startup_scene_and_exports_package() {
        TextWriter previousOutput = Console.Out;
        StringWriter output = new StringWriter();
        string workingRootPath = Path.Combine(Path.GetTempPath(), "helengine-3ds-verification-" + Guid.NewGuid().ToString("N"));

        try {
            FakeNintendo3DsNativeBuildExecutor nativeBuildExecutor = new();
            Console.SetOut(output);

            Nintendo3DsVerificationHarness.RunSmokeTest(workingRootPath, nativeBuildExecutor);

            Assert.Contains("Smoke test passed.", output.ToString(), StringComparison.Ordinal);
            Assert.NotNull(nativeBuildExecutor.Workspace);
            Assert.Equal(Path.GetFullPath(Path.Combine(workingRootPath, "out")), nativeBuildExecutor.Workspace.OutputRootPath);
            Assert.True(File.Exists(nativeBuildExecutor.Workspace.ExportPackagePath));
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "runtime", "3ds_startup_manifest.bin")));
            Assert.True(File.Exists(Path.Combine(nativeBuildExecutor.Workspace.RomFsRootPath, "cooked", "scenes", "mainmenu.hasset")));
        } finally {
            Console.SetOut(previousOutput);
            if (Directory.Exists(workingRootPath)) {
                Directory.Delete(workingRootPath, recursive: true);
            }

            output.Dispose();
        }
    }
}
