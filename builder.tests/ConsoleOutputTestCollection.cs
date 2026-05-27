namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Groups tests that temporarily replace <see cref="Console.Out"/> so xUnit does not run them concurrently.
/// </summary>
[CollectionDefinition(Name)]
public sealed class ConsoleOutputTestCollection {
    /// <summary>
    /// Stable xUnit collection name for console-output-sensitive tests.
    /// </summary>
    public const string Name = "Console output";
}
