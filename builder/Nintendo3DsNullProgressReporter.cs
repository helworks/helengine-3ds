using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Provides a no-op progress reporter for builder-owned smoke verification.
/// </summary>
internal sealed class Nintendo3DsNullProgressReporter : IPlatformBuildProgressReporter {
    /// <summary>
    /// Ignores one progress update emitted by the builder.
    /// </summary>
    /// <param name="update">Progress update to ignore.</param>
    public void Report(PlatformBuildProgressUpdate update) {
    }
}
