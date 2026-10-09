using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.Versioning;
using helengine.editor;

namespace helengine.nintendo3ds.builder.tests;

/// <summary>Checks native PICA words, tiles, ETC blocks, runtime serialization and external tex3ds containers.</summary>
public sealed class Nintendo3DsNativeTextureTests {
    /// <summary>Enumerates every hardware format with intrinsic alpha and opaque cooking.</summary>
    public static IEnumerable<object[]> Formats() {
        foreach (Nintendo3DsTextureFormat format in Nintendo3DsTextureFormatCatalog.Formats) {
            yield return new object[] { format.Id, format.Alpha };
            if (format.Alpha != TextureAssetAlphaPrecision.Opaque) yield return new object[] { format.Id, TextureAssetAlphaPrecision.Opaque };
        }
    }
    /// <summary>Checks cooking, exact GPU budgets, file/runtime readers and malformed native descriptors.</summary>
    [Theory]
    [MemberData(nameof(Formats))]
    public void Native_formats_cook_preview_and_roundtrip(string id, TextureAssetAlphaPrecision alpha) {
        using TextureAsset source = Source(13, 9);
        var settings = new helengine.nintendo3ds.builder.TextureAssetProcessorSettings { ColorFormatId = id, AlphaPrecision = alpha, MaxResolution = 1024 };
        Assert.Equal(id, Nintendo3DsTextureCookSettingsSerializer.Deserialize(Nintendo3DsTextureCookSettingsSerializer.Serialize(settings)).ColorFormatId);
        using TextureAsset native = Nintendo3DsTextureCooker.Apply(source, settings);
        Nintendo3DsTextureLayout layout = Nintendo3DsTextureCodec.ReadLayout(native);
        byte[] pixels = TextureAssetPixelCodec.DecodeToRgba32(native);
        Assert.Equal(13 * 9 * 4, pixels.Length);
        Assert.Equal(source.Id, native.Id); Assert.Equal(source.RuntimeAssetId, native.RuntimeAssetId);
        Assert.True(TextureVramUsageCalculator.TryCalculateBytes(13, 9, new helengine.editor.TextureAssetProcessorSettings { ColorFormatId = id, AlphaPrecision = alpha }, out long budget));
        Assert.Equal(layout.TexelBytes, budget);
        using MemoryStream stream = new MemoryStream(global::helengine.files.AssetSerializer.SerializeToBytes(native));
        using TextureAsset file = Assert.IsType<TextureAsset>(global::helengine.files.AssetSerializer.Deserialize(stream));
        Assert.Equal(pixels, Nintendo3DsTextureCodec.Decode(file));
        stream.Position = 0;
        using TextureAsset runtime = Assert.IsType<TextureAsset>(PackagedAssetBinarySerializer.Deserialize(stream));
        Assert.Equal(native.Colors, runtime.Colors);
        Assert.Equal(pixels, Nintendo3DsTextureCodec.Decode(runtime));
        foreach (int offset in new[] { 0, 4, 8, 12, 16, 20, 24, 28 }) {
            byte old = native.Colors[offset]; native.Colors[offset] = 255;
            Assert.ThrowsAny<ArgumentException>(() => Nintendo3DsTextureCodec.ReadLayout(native));
            native.Colors[offset] = old;
        }
        if (alpha == layout.Format.Alpha) {
            string path = Path.Combine(OutputRoot(), id);
            File.WriteAllBytes(path + ".n3t", native.Colors);
            File.WriteAllBytes(path + ".rgba", pixels);
        }
    }
    /// <summary>Checks native packed red independently from its inverse decoder.</summary>
    [Theory]
    [InlineData("Nintendo3Ds.Rgba8", new byte[] { 255, 0, 0, 255 })]
    [InlineData("Nintendo3Ds.Rgb8", new byte[] { 0, 0, 255 })]
    [InlineData("Nintendo3Ds.Rgba5551", new byte[] { 1, 248 })]
    [InlineData("Nintendo3Ds.Rgb565", new byte[] { 0, 248 })]
    [InlineData("Nintendo3Ds.Rgba4", new byte[] { 15, 240 })]
    public void Packed_red_has_high_color_bits(string id, byte[] expected) {
        using TextureAsset source = new TextureAsset { Width = 1, Height = 1, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8, Colors = new byte[] { 255, 0, 0, 255 } };
        using TextureAsset native = Nintendo3DsTextureCodec.Encode(source, id, Nintendo3DsTextureFormatCatalog.Formats.First(item => item.Id == id).Alpha);
        Assert.Equal(expected, native.Colors.Skip(32).Take(expected.Length));
    }
    /// <summary>Checks Morton tile boundaries and the low-nibble-first four-bit packing.</summary>
    [Fact]
    public void Morton_tiles_and_four_bit_texels_have_literal_addresses() {
        Assert.Equal(0, Nintendo3DsTextureLayout.PixelIndex(0, 0, 16));
        Assert.Equal(1, Nintendo3DsTextureLayout.PixelIndex(1, 0, 16));
        Assert.Equal(2, Nintendo3DsTextureLayout.PixelIndex(0, 1, 16));
        Assert.Equal(64, Nintendo3DsTextureLayout.PixelIndex(8, 0, 16));
        Assert.Equal(128, Nintendo3DsTextureLayout.PixelIndex(0, 8, 16));
        using TextureAsset source = new TextureAsset { Width = 2, Height = 1, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8, Colors = new byte[] { 255, 255, 255, 255, 0, 0, 0, 255 } };
        using TextureAsset native = Nintendo3DsTextureCodec.Encode(source, "Nintendo3Ds.L4", TextureAssetAlphaPrecision.Opaque);
        Assert.Equal(0x0f, native.Colors[32]);
    }
    /// <summary>Checks column-major alpha nibbles and a literal ETC1 individual-mode color block.</summary>
    [Fact]
    public void Etc1a4_uses_little_endian_color_and_column_major_alpha() {
        byte[] block = new byte[] { 0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe, 0, 0, 0, 0, 0, 0, 0, 0xff };
        byte[] rgba = new byte[4];
        Nintendo3DsEtcTextureCodec.Sample(block, 0, 13, 2, 1, rgba, 0);
        Assert.Equal(new byte[] { 255, 2, 2, 153 }, rgba);
    }
    /// <summary>Exercises an external differential-mode block and rejects differential endpoints belonging to ETC2.</summary>
    [Fact]
    public void Etc1_differential_mode_uses_signed_three_bit_deltas() {
        byte[] block = new byte[] { 0, 0, 0, 0, 2, 0x88, 0x88, 0x89 };
        byte[] rgba = new byte[4];
        Nintendo3DsEtcTextureCodec.Sample(block, 0, 12, 3, 0, rgba, 0);
        Assert.Equal(new byte[] { 150, 142, 142, 255 }, rgba);
        block[7] = 0xfb;
        Assert.Throws<ArgumentException>(() => Nintendo3DsEtcTextureCodec.Sample(block, 0, 12, 3, 0, rgba, 0));
    }
    /// <summary>Checks published settings and preserves the existing default image and font contracts.</summary>
    [Fact]
    public void Capabilities_publish_all_native_settings_and_existing_defaults() {
        var definition = Nintendo3DsPlatformDefinitionFactory.Create();
        foreach (var capability in definition.AssetCookCapabilities.Where(item => item.SourceAssetKind == "texture" || item.SourceAssetKind == "font-atlas-texture")) {
            Assert.Equal(15, capability.TextureFormatCapabilities.SupportedColorFormatIds.Length);
            foreach (Nintendo3DsTextureFormat format in Nintendo3DsTextureFormatCatalog.Formats) Assert.Contains(format.Id, capability.TextureFormatCapabilities.SupportedColorFormatIds);
        }
        Assert.Throws<ArgumentException>(() => Nintendo3DsTextureCooker.ValidateSettings(new helengine.nintendo3ds.builder.TextureAssetProcessorSettings { ColorFormatId = "Nintendo3Ds.Rgb565", AlphaPrecision = TextureAssetAlphaPrecision.A8 }));
        Assert.Throws<ArgumentException>(() => Nintendo3DsTextureCooker.ValidateSettings(new helengine.nintendo3ds.builder.TextureAssetProcessorSettings { MaxResolution = 1025 }));
        Assert.Throws<ArgumentOutOfRangeException>(() => new Nintendo3DsTextureLayout(Nintendo3DsTextureFormatCatalog.Formats[0], 1025, 1));
    }
    /// <summary>Creates the real image used by the external tex3ds corpus generation script.</summary>
    [Fact]
    [SupportedOSPlatform("windows")]
    public void Write_external_tex3ds_source_image() {
        using Bitmap bitmap = new Bitmap(16, 16);
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) bitmap.SetPixel(x, y, Color.FromArgb(85 + (x + y) % 3 * 85, x % 8 < 4 ? 255 : 0, y % 8 < 4 ? 255 : 0, (x + y) % 8 < 4 ? 255 : 0));
        }
        bitmap.Save(Path.Combine(OutputRoot(), "external-source.png"), ImageFormat.Png);
    }
    /// <summary>Creates deterministic RGBA source pixels that exercise padding and intrinsic alpha.</summary>
    static TextureAsset Source(int width, int height) {
        byte[] rgba = new byte[width * height * 4];
        for (int index = 0; index < width * height; index++) {
            rgba[index * 4] = (byte)(index * 37); rgba[index * 4 + 1] = (byte)(index * 17); rgba[index * 4 + 2] = (byte)(index * 11); rgba[index * 4 + 3] = (byte)(index * 23);
        }
        return new TextureAsset { Id = "3ds-texture-fixture", RuntimeAssetId = 25, Width = (ushort)width, Height = (ushort)height, ColorFormat = TextureAssetColorFormat.Rgba32, AlphaPrecision = TextureAssetAlphaPrecision.A8, Colors = rgba };
    }
    /// <summary>Resolves visible, workspace-owned fixtures for both managed and native validation.</summary>
    static string OutputRoot() {
        string root = Environment.GetEnvironmentVariable("HELENGINE_3DS_TEXTURE_TEST_OUTPUT") ?? Path.Combine(AppContext.BaseDirectory, "3ds-texture-fixtures");
        Directory.CreateDirectory(root); return root;
    }
}
