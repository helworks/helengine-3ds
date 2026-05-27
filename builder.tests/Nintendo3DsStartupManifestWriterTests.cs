namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Verifies the Nintendo 3DS startup-manifest writer byte contract and validation behavior.
/// </summary>
public class Nintendo3DsStartupManifestWriterTests {
    /// <summary>
    /// Verifies the writer emits the expected staged RomFS path and binary payload.
    /// </summary>
    [Fact]
    public void Write_creates_expected_manifest_bytes() {
        string workingRoot = Path.Combine(Path.GetTempPath(), "helengine-3ds-manifest-" + Guid.NewGuid().ToString("N"));

        try {
            Nintendo3DsStartupManifestWriter writer = new();
            string romFsRootPath = Path.Combine(workingRoot, "romfs");

            string manifestPath = writer.Write(
                romFsRootPath,
                topScreenColorHex: "#FF0000",
                bottomScreenColorHex: "#0000FF");

            byte[] bytes = File.ReadAllBytes(manifestPath);

            Assert.Equal(Path.Combine(romFsRootPath, "runtime", "3ds_startup_manifest.bin"), manifestPath);
            Assert.Equal(
                [
                    0x48, 0x33, 0x53, 0x50,
                    0x01, 0x00,
                    0x08, 0x00,
                    0xFF, 0x00, 0x00, 0xFF,
                    0x00, 0x00, 0xFF, 0xFF
                ],
                bytes);
        } finally {
            if (Directory.Exists(workingRoot)) {
                Directory.Delete(workingRoot, recursive: true);
            }
        }
    }

    /// <summary>
    /// Verifies invalid authored color text fails with an explicit top-screen setting message.
    /// </summary>
    [Fact]
    public void Write_when_color_text_is_invalid_throws() {
        Nintendo3DsStartupManifestWriter writer = new();
        string romFsRootPath = Path.Combine(Path.GetTempPath(), "helengine-3ds-manifest-invalid-" + Guid.NewGuid().ToString("N"));

        try {
            InvalidOperationException exception = Assert.Throws<InvalidOperationException>(() =>
                writer.Write(romFsRootPath, "#12", "#0000FF"));
            Assert.Contains("startup top screen color", exception.Message, StringComparison.OrdinalIgnoreCase);
        } finally {
            if (Directory.Exists(romFsRootPath)) {
                Directory.Delete(romFsRootPath, recursive: true);
            }
        }
    }
}
