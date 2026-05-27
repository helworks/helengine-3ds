using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Provides a no-op diagnostic reporter for builder-owned smoke verification.
/// </summary>
internal sealed class Nintendo3DsNullDiagnosticReporter : IPlatformBuildDiagnosticReporter {
    /// <summary>
    /// Ignores one diagnostic emitted by the builder.
    /// </summary>
    /// <param name="diagnostic">Diagnostic to ignore.</param>
    public void Report(PlatformBuildDiagnostic diagnostic) {
    }
}
