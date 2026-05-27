using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.nintendo3ds.builder.tests.Builders;

/// <summary>
/// Records streamed diagnostics emitted by the builder under test.
/// </summary>
public sealed class RecordingDiagnosticReporter : IPlatformBuildDiagnosticReporter {
    /// <summary>
    /// Gets the captured diagnostics in report order.
    /// </summary>
    public List<PlatformBuildDiagnostic> Diagnostics { get; } = [];

    /// <summary>
    /// Records one diagnostic.
    /// </summary>
    /// <param name="diagnostic">Diagnostic to capture.</param>
    public void Report(PlatformBuildDiagnostic diagnostic) {
        Diagnostics.Add(diagnostic);
    }
}
