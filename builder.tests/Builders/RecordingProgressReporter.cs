using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.nintendo3ds.builder.tests.Builders;

/// <summary>
/// Records streamed progress updates emitted by the builder under test.
/// </summary>
public sealed class RecordingProgressReporter : IPlatformBuildProgressReporter {
    /// <summary>
    /// Gets the captured progress updates in report order.
    /// </summary>
    public List<PlatformBuildProgressUpdate> Updates { get; } = [];

    /// <summary>
    /// Records one progress update.
    /// </summary>
    /// <param name="update">Progress update to capture.</param>
    public void Report(PlatformBuildProgressUpdate update) {
        Updates.Add(update);
    }
}
