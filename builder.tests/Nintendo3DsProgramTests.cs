namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Verifies the Nintendo 3DS builder command-line entrypoints.
/// </summary>
[Collection(ConsoleOutputTestCollection.Name)]
public class Nintendo3DsProgramTests {
    /// <summary>
    /// Verifies the help text includes the smoke-test command.
    /// </summary>
    [Fact]
    public void Main_whenNoArgumentsAreProvided_prints_smoke_test_usage() {
        TextWriter previousOutput = Console.Out;
        StringWriter output = new StringWriter();

        try {
            Console.SetOut(output);

            int exitCode = Program.Main([]);

            Assert.Equal(0, exitCode);
            Assert.Contains("helengine.3ds.builder --describe | --smoke-test", output.ToString(), StringComparison.Ordinal);
        } finally {
            Console.SetOut(previousOutput);
            output.Dispose();
        }
    }
}
