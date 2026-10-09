using System.Drawing;
using System.Runtime.Versioning;
using helengine.editor;

namespace helengine.nintendo3ds.builder.tests;

/// <summary>Validates actual tex3ds output across every PICA encoding and compression option.</summary>
[SupportedOSPlatform("windows")]
public sealed class Nintendo3DsT3xTextureTests {
    /// <summary>Enumerates the external corpus produced by the installed tex3ds tool.</summary>
    public static IEnumerable<object[]> Corpus() {
        foreach (string format in new[] { "rgba8", "rgb8", "rgba5551", "rgb565", "rgba4", "la8", "hilo8", "l8", "a8", "la4", "l4", "a4", "etc1", "etc1a4" }) {
            foreach (string compression in new[] { "none", "lz10", "lz11", "rle", "huff", "auto" }) yield return new object[] { format, compression };
        }
    }
    /// <summary>Compares imported pixels with tex3ds's independent PNG preview, allowing one component-expansion rounding bit.</summary>
    [Theory]
    [MemberData(nameof(Corpus))]
    public void External_containers_match_tex3ds_previews(string format, string compression) {
        string prefix = Path.Combine(CorpusRoot(), format + "-" + compression);
        using FileStream stream = File.OpenRead(prefix + ".t3x");
        using TextureAsset imported = new Nintendo3DsT3xTextureImporter().ImportTexture(stream);
        using Bitmap preview = new Bitmap(Path.Combine(CorpusRoot(), format + ".png"));
        Assert.Equal(preview.Width, imported.Width); Assert.Equal(preview.Height, imported.Height);
        for (int y = 0; y < preview.Height; y++) {
            for (int x = 0; x < preview.Width; x++) {
                Color expected = preview.GetPixel(x, y);
                int offset = (y * preview.Width + x) * 4;
                Assert.InRange(Math.Abs(expected.A - imported.Colors[offset + 3]), 0, 1);
                if (expected.A == 0) continue;
                Assert.InRange(Math.Abs(expected.R - imported.Colors[offset]), 0, 1);
                Assert.InRange(Math.Abs(expected.G - imported.Colors[offset + 1]), 0, 1);
                Assert.InRange(Math.Abs(expected.B - imported.Colors[offset + 2]), 0, 1);
            }
        }
    }
    /// <summary>Checks base-level mip selection and strict container/compression metadata validation.</summary>
    [Fact]
    public void Mips_import_base_level_and_malformed_headers_fail() {
        byte[] bytes = File.ReadAllBytes(Path.Combine(CorpusRoot(), "rgba8-mips.t3x"));
        using TextureAsset imported = new Nintendo3DsT3xTextureImporter().ImportTexture(new MemoryStream(bytes));
        Assert.Equal(16, imported.Width); Assert.Equal(16, imported.Height);
        foreach (int offset in new[] { 0, 2, 3, 4, 5, 9, 17, 18 }) {
            byte old = bytes[offset]; bytes[offset] = 255;
            Assert.ThrowsAny<Exception>(() => new Nintendo3DsT3xTextureImporter().ImportTexture(new MemoryStream(bytes)));
            bytes[offset] = old;
        }
        Assert.ThrowsAny<Exception>(() => new Nintendo3DsT3xTextureImporter().ImportTexture(new MemoryStream(bytes.Take(bytes.Length - 4).ToArray())));
    }
    /// <summary>Runs the actual source processor on a compressed native container and generated font-atlas texture.</summary>
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void Source_processor_imports_and_cooks_real_t3x(bool fontAtlas) {
        string source = Path.Combine(CorpusRoot(), "rgba8-auto.t3x");
        var settings = new helengine.nintendo3ds.builder.TextureAssetProcessorSettings { MaxResolution = 8, ColorFormatId = "Nintendo3Ds.Etc1A4", AlphaPrecision = TextureAssetAlphaPrecision.A4 };
        Nintendo3DsPlatformCookSourceProcessor processor = new Nintendo3DsPlatformCookSourceProcessor();
        using TextureAsset cooked = fontAtlas ? processor.CookFontAtlasTexture(source, "font:3ds", settings) : processor.CookTexture(source, "texture:3ds", settings);
        Assert.Equal(8, cooked.Width); Assert.Equal(8, cooked.Height);
        Assert.Equal(fontAtlas ? "font:3ds" : "texture:3ds", cooked.Id);
        Assert.Equal(13, Nintendo3DsTextureCodec.ReadLayout(cooked).Format.Code);
    }
    /// <summary>Checks ordinary and rotated atlas rectangles against independent tex3ds preview coordinates.</summary>
    [Theory]
    [InlineData(false)]
    [InlineData(true)]
    public void Atlas_rectangles_preserve_authored_orientation(bool rotated) {
        byte[] bytes = File.ReadAllBytes(Path.Combine(CorpusRoot(), "rgba8-none.t3x"));
        bytes[5] = (byte)(rotated ? 16 : 8); bytes[6] = 0;
        bytes[7] = (byte)(rotated ? 8 : 16); bytes[8] = 0;
        bytes[9] = 0; bytes[10] = 0;
        bytes[11] = 0; bytes[12] = (byte)(rotated ? 0 : 4);
        bytes[13] = 0; bytes[14] = (byte)(rotated ? 4 : 2);
        bytes[15] = 0; bytes[16] = (byte)(rotated ? 2 : 0);
        using TextureAsset imported = new Nintendo3DsT3xTextureImporter().ImportTexture(new MemoryStream(bytes));
        using Bitmap preview = new Bitmap(Path.Combine(CorpusRoot(), "rgba8.png"));
        for (int y = 0; y < imported.Height; y++) {
            for (int x = 0; x < imported.Width; x++) {
                Color expected = preview.GetPixel(rotated ? y : x, rotated ? 15 - x : y);
                int target = (y * imported.Width + x) * 4;
                Assert.Equal(new byte[] { expected.R, expected.G, expected.B, expected.A }, imported.Colors.Skip(target).Take(4).ToArray());
            }
        }
    }
    /// <summary>Resolves checked-in independent fixtures so ordinary tests do not require Docker or tex3ds.</summary>
    static string CorpusRoot() => Path.Combine(AppContext.BaseDirectory, "Fixtures", "tex3ds");
}
