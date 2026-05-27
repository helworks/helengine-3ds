namespace helengine.nintendo3ds.builder;

/// <summary>
/// Writes the builder-owned Nintendo 3DS startup manifest into a staged RomFS root.
/// </summary>
public sealed class Nintendo3DsStartupManifestWriter {
    /// <summary>
    /// Stores the stable runtime-relative RomFS path used for the startup manifest payload.
    /// </summary>
    const string ManifestRelativePath = "runtime/3ds_startup_manifest.bin";

    /// <summary>
    /// Writes one startup manifest into the staged RomFS root.
    /// </summary>
    /// <param name="romFsRootPath">RomFS root directory that receives the manifest.</param>
    /// <param name="topScreenColorHex">Top-screen color in <c>#RRGGBB</c> form.</param>
    /// <param name="bottomScreenColorHex">Bottom-screen color in <c>#RRGGBB</c> form.</param>
    /// <returns>Full path to the emitted startup-manifest file.</returns>
    public string Write(string romFsRootPath, string topScreenColorHex, string bottomScreenColorHex) {
        if (string.IsNullOrWhiteSpace(romFsRootPath)) {
            throw new ArgumentException("RomFS root path must be provided.", nameof(romFsRootPath));
        }

        uint topScreenColor = ParseColor(topScreenColorHex, "startup top screen color");
        uint bottomScreenColor = ParseColor(bottomScreenColorHex, "startup bottom screen color");

        string manifestPath = Path.Combine(romFsRootPath, ManifestRelativePath.Replace('/', Path.DirectorySeparatorChar));
        string manifestDirectoryPath = Path.GetDirectoryName(manifestPath)
            ?? throw new InvalidOperationException("Unable to resolve the Nintendo 3DS startup manifest directory.");
        Directory.CreateDirectory(manifestDirectoryPath);

        using FileStream stream = File.Create(manifestPath);
        using BinaryWriter writer = new(stream);
        writer.Write(new byte[] { 0x48, 0x33, 0x53, 0x50 });
        writer.Write((ushort)1);
        writer.Write((ushort)8);
        writer.Write(topScreenColor);
        writer.Write(bottomScreenColor);

        return manifestPath;
    }

    /// <summary>
    /// Parses one authored RGB color string into the packed Nintendo 3DS RGBA clear-color format.
    /// </summary>
    /// <param name="colorText">Authored color string in <c>#RRGGBB</c> form.</param>
    /// <param name="fieldName">Human-readable field name used in validation errors.</param>
    /// <returns>Packed Nintendo 3DS color value.</returns>
    static uint ParseColor(string colorText, string fieldName) {
        if (string.IsNullOrWhiteSpace(colorText)) {
            throw new InvalidOperationException($"The {fieldName} build setting is required.");
        }

        string normalized = colorText.Trim();
        if (normalized.StartsWith("#", StringComparison.Ordinal)) {
            normalized = normalized.Substring(1);
        }

        if (normalized.Length != 6) {
            throw new InvalidOperationException($"The {fieldName} build setting must use #RRGGBB format.");
        }

        try {
            byte red = Convert.ToByte(normalized.Substring(0, 2), 16);
            byte green = Convert.ToByte(normalized.Substring(2, 2), 16);
            byte blue = Convert.ToByte(normalized.Substring(4, 2), 16);
            return red | ((uint)green << 8) | ((uint)blue << 16) | (0xFFu << 24);
        } catch (FormatException ex) {
            throw new InvalidOperationException($"The {fieldName} build setting must use #RRGGBB format.", ex);
        } catch (OverflowException ex) {
            throw new InvalidOperationException($"The {fieldName} build setting must use #RRGGBB format.", ex);
        }
    }
}
