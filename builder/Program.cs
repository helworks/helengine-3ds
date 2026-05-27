using System.Runtime.Versioning;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Provides a small command-line entrypoint for the Nintendo 3DS builder assembly.
/// </summary>
[SupportedOSPlatform("windows")]
public static class Program {
    /// <summary>
    /// Prints builder metadata when requested.
    /// </summary>
    /// <param name="args">Command-line arguments.</param>
    /// <returns>Zero on success.</returns>
    public static int Main(string[] args) {
        if (args.Length > 0 && string.Equals(args[0], "--describe", StringComparison.OrdinalIgnoreCase)) {
            Nintendo3DsPlatformAssetBuilder builder = new();
            Console.WriteLine(builder.Descriptor.BuilderId);
            Console.WriteLine(builder.Descriptor.TargetPlatformId);
            Console.WriteLine(builder.Definition.DisplayName);
            return 0;
        }

        if (args.Length > 0 && string.Equals(args[0], "--smoke-test", StringComparison.OrdinalIgnoreCase)) {
            try {
                Nintendo3DsVerificationHarness.RunSmokeTest();
                return 0;
            } catch (Exception exception) {
                Console.Error.WriteLine(exception.Message);
                return 1;
            }
        }

        Console.WriteLine("helengine.3ds.builder --describe | --smoke-test");
        return 0;
    }
}
