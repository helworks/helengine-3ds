using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Imports raw source font files into RGBA atlas textures that match the editor font-atlas layout used by Nintendo 3DS builder-owned cooking.
/// </summary>
[SupportedOSPlatform("windows")]
public sealed class Nintendo3DsSourceFontAtlasTextureImporter {
    /// <summary>
    /// Extra transparent pixels around each glyph in the atlas to prevent sampling bleed.
    /// </summary>
    const int AtlasPadding = 2;

    /// <summary>
    /// Stable glyph set emitted by the editor font atlas generator.
    /// </summary>
    static readonly char[] Characters = [
        'a', '\u00E0', '\u00E1', '\u00E3', 'b', 'c', 'd', 'e', '\u00E9', '\u00EA',
        'f', 'g', 'h', 'i', '\u00ED', 'j',
        'k', 'l', 'm', 'n', 'o', '\u00F3', '\u00F4', '\u00F5',
        'p', 'q', 'r', 's', 't',
        'u', '\u00FA', 'v', 'w', 'x', 'y', 'z',
        'A', '\u00C0', '\u00C1', '\u00C3', 'B', 'C', 'D', 'E', '\u00C9', '\u00CA',
        'F', 'G', 'H', 'I', '\u00CD', 'J',
        'K', 'L', 'M', 'N', 'O', '\u00D3', '\u00D4', '\u00D5',
        'P', 'Q', 'R', 'S', 'T',
        'U', '\u00DA', 'V', 'W', 'X', 'Y', 'Z', '\u00C7', '\u00E7',
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
        '!', '@', '#', '$', '%', '^', '&', '*',
        '(', ')', '-', '_', '=', '+', '?', ',', '.',
        '/', '\\', ':', ';', '|',
        '{', '}', '~', '`', '\''
    ];

    /// <summary>
    /// Imports one raw source font file into one RGBA atlas texture asset.
    /// </summary>
    /// <param name="sourceAssetPath">Absolute raw source font path.</param>
    /// <returns>Raw RGBA atlas texture asset.</returns>
    public TextureAsset Import(string sourceAssetPath) {
        if (string.IsNullOrWhiteSpace(sourceAssetPath)) {
            throw new ArgumentException("Source font path must be provided.", nameof(sourceAssetPath));
        } else if (!File.Exists(sourceAssetPath)) {
            throw new FileNotFoundException("Source font file was not found.", sourceAssetPath);
        }

        using FileStream stream = File.OpenRead(sourceAssetPath);
        return Import(stream);
    }

    /// <summary>
    /// Imports one raw source font stream into one RGBA atlas texture asset.
    /// </summary>
    /// <param name="stream">Stream containing the source font bytes.</param>
    /// <returns>Raw RGBA atlas texture asset.</returns>
    public TextureAsset Import(Stream stream) {
        if (stream == null) {
            throw new ArgumentNullException(nameof(stream));
        }

        using MemoryStream buffer = new();
        stream.CopyTo(buffer);
        byte[] bytes = buffer.ToArray();
        if (bytes.Length == 0) {
            throw new InvalidOperationException("Font source stream must contain data.");
        }

        nint nativeBuffer = Marshal.AllocCoTaskMem(bytes.Length);
        try {
            Marshal.Copy(bytes, 0, nativeBuffer, bytes.Length);
            using PrivateFontCollection fontCollection = new();
            fontCollection.AddMemoryFont(nativeBuffer, bytes.Length);
            if (fontCollection.Families.Length == 0) {
                throw new InvalidOperationException("Source font did not produce any installable font families.");
            }

            using Font font = new(fontCollection.Families[0], 32f, FontStyle.Regular, GraphicsUnit.Pixel);
            return Import(font);
        } finally {
            Marshal.FreeCoTaskMem(nativeBuffer);
        }
    }

    /// <summary>
    /// Imports one font instance into one RGBA atlas texture asset using the editor atlas layout algorithm.
    /// </summary>
    /// <param name="font">Font instance to rasterize.</param>
    /// <returns>Raw RGBA atlas texture asset.</returns>
    TextureAsset Import(Font font) {
        if (font == null) {
            throw new ArgumentNullException(nameof(font));
        }

        Dictionary<char, Nintendo3DsFontAtlasGlyph> glyphs = [];

        float lineHeightPixels;
        using (Bitmap measurementBitmap = new(1, 1))
        using (Graphics graphics = Graphics.FromImage(measurementBitmap)) {
            lineHeightPixels = font.GetHeight(graphics);
        }

        int resolution = Math.Max(16, (int)Math.Ceiling(lineHeightPixels));
        int offset = (int)(resolution * 0.1f);

        for (int characterIndex = 0; characterIndex < Characters.Length; characterIndex++) {
            char character = Characters[characterIndex];
            GenerateCharacterBitmap(character, font, resolution * 2, offset, out Bitmap bitmap, out Rectangle rectangle);
            glyphs[character] = new Nintendo3DsFontAtlasGlyph(
                new int4(rectangle.X, rectangle.Y, rectangle.Width, rectangle.Height),
                bitmap);
        }

        using Bitmap atlas = GenerateAtlas(glyphs);
        return ConvertAtlasBitmapToTextureAsset(atlas);
    }

    /// <summary>
    /// Generates one glyph bitmap and bounds for the specified character.
    /// </summary>
    /// <param name="character">Character to generate.</param>
    /// <param name="font">Font used for rendering.</param>
    /// <param name="resolution">Resolution of the temporary bitmap.</param>
    /// <param name="offset">Padding offset applied before rendering.</param>
    /// <param name="bitmap">Output bitmap containing the rendered glyph.</param>
    /// <param name="rectangle">Output bounds of the glyph within the bitmap.</param>
    static void GenerateCharacterBitmap(
        char character,
        Font font,
        int resolution,
        int offset,
        out Bitmap bitmap,
        out Rectangle rectangle) {
        bitmap = new Bitmap(resolution, resolution);
        string characterString = character.ToString();
        int position = offset;
        using (Graphics graphics = Graphics.FromImage(bitmap)) {
            graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.None;
            graphics.TextRenderingHint = TextRenderingHint.SingleBitPerPixelGridFit;
            graphics.DrawString(characterString, font, Brushes.White, new PointF(position, position), StringFormat.GenericTypographic);
        }

        Nintendo3DsBitmapLock bitmapLock = new(bitmap);
        bitmapLock.LockBits();

        byte[,] bytes = new byte[resolution, resolution];
        for (int x = 0; x < bitmap.Width; x++) {
            for (int y = 0; y < bitmap.Height; y++) {
                Color pixel = bitmapLock.GetPixel(x, y);
                bytes[x, y] = pixel.R;
            }
        }

        bitmapLock.UnlockBits(false);

        int minX = int.MaxValue;
        int minY = int.MaxValue;
        int maxX = int.MinValue;
        int maxY = int.MinValue;
        for (int x = 0; x < resolution; x++) {
            for (int y = 0; y < resolution; y++) {
                if (bytes[x, y] == 0) {
                    continue;
                }

                minX = Math.Min(minX, x);
                minY = Math.Min(minY, y);
                maxX = Math.Max(maxX, x);
                maxY = Math.Max(maxY, y);
            }
        }

        if (minX == int.MaxValue || minY == int.MaxValue) {
            rectangle = new Rectangle(0, 0, 1, 1);
            return;
        }

        int width = (maxX - minX) + 1;
        int height = (maxY - minY) + 1;
        int expansion = 1;
        rectangle = new Rectangle(minX - expansion, minY - expansion, width + expansion, height + expansion);
    }

    /// <summary>
    /// Builds one packed atlas bitmap from the temporary glyph data.
    /// </summary>
    /// <param name="glyphs">Source glyph definitions keyed by character.</param>
    /// <returns>Generated atlas bitmap.</returns>
    static Bitmap GenerateAtlas(Dictionary<char, Nintendo3DsFontAtlasGlyph> glyphs) {
        if (glyphs == null) {
            throw new ArgumentNullException(nameof(glyphs));
        }

        List<Nintendo3DsFontAtlasGlyphItem> items = glyphs
            .Select(entry => new Nintendo3DsFontAtlasGlyphItem {
                Character = entry.Key,
                SourceX = entry.Value.SourceRect.X,
                SourceY = entry.Value.SourceRect.Y,
                Width = entry.Value.SourceRect.Z,
                Height = entry.Value.SourceRect.W,
                Bitmap = entry.Value.Bitmap
            })
            .OrderByDescending(item => item.Height)
            .ToList();

        Size atlasSize = CalculateOptimalAtlasSize(items);
        Dictionary<char, Point> positions = CalculatePositions(items, atlasSize.Width);
        return CreateAtlasBitmap(items, positions, atlasSize.Width, atlasSize.Height);
    }

    /// <summary>
    /// Calculates one optimal power-of-two atlas size for the supplied glyphs.
    /// </summary>
    /// <param name="items">Glyph items with dimensions.</param>
    /// <returns>Width and height for the atlas bitmap.</returns>
    static Size CalculateOptimalAtlasSize(List<Nintendo3DsFontAtlasGlyphItem> items) {
        if (items == null) {
            throw new ArgumentNullException(nameof(items));
        } else if (items.Count == 0) {
            return new Size(1, 1);
        }

        const int maximumSize = 2048;
        int maximumItemWidth = items.Max(item => item.Width + (AtlasPadding * 2));
        List<Size> candidates = [];
        for (int powerOfTwo = 32; powerOfTwo <= maximumSize; powerOfTwo *= 2) {
            if (powerOfTwo < maximumItemWidth) {
                continue;
            }

            int requiredHeight = CalculateRequiredHeight(items, powerOfTwo);
            if (requiredHeight <= powerOfTwo) {
                candidates.Add(new Size(powerOfTwo, requiredHeight));
            }
        }

        Size best = candidates
            .OrderBy(candidate => Math.Max(candidate.Width, candidate.Height))
            .ThenBy(candidate => candidate.Width * candidate.Height)
            .FirstOrDefault();
        if (best.Width == 0) {
            best = new Size(maximumSize, Math.Min(CalculateRequiredHeight(items, maximumSize), maximumSize));
        }

        return new Size(NextPowerOfTwo(best.Width), NextPowerOfTwo(best.Height));
    }

    /// <summary>
    /// Finds the next power-of-two greater than or equal to the supplied value.
    /// </summary>
    /// <param name="value">Input value.</param>
    /// <returns>Next power-of-two.</returns>
    static int NextPowerOfTwo(int value) {
        if (value < 1) {
            return 1;
        }

        value--;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        return value + 1;
    }

    /// <summary>
    /// Estimates the required atlas height for the supplied width using row packing.
    /// </summary>
    /// <param name="items">Glyph items to pack.</param>
    /// <param name="atlasWidth">Candidate atlas width.</param>
    /// <returns>Estimated height required to fit all glyphs.</returns>
    static int CalculateRequiredHeight(List<Nintendo3DsFontAtlasGlyphItem> items, int atlasWidth) {
        if (items == null) {
            throw new ArgumentNullException(nameof(items));
        }

        int currentX = 0;
        int currentY = 0;
        int rowHeight = 0;
        int totalHeight = 0;
        for (int itemIndex = 0; itemIndex < items.Count; itemIndex++) {
            Nintendo3DsFontAtlasGlyphItem item = items[itemIndex];
            int paddedWidth = item.Width + (AtlasPadding * 2);
            int paddedHeight = item.Height + (AtlasPadding * 2);
            if (currentX + paddedWidth > atlasWidth) {
                currentY += rowHeight;
                currentX = 0;
                rowHeight = 0;
            }

            rowHeight = Math.Max(rowHeight, paddedHeight);
            currentX += paddedWidth;
            totalHeight = currentY + rowHeight;
        }

        return totalHeight;
    }

    /// <summary>
    /// Calculates top-left positions for each glyph within the atlas.
    /// </summary>
    /// <param name="items">Glyph items to position.</param>
    /// <param name="atlasWidth">Width of the atlas.</param>
    /// <returns>Dictionary mapping characters to their atlas positions.</returns>
    static Dictionary<char, Point> CalculatePositions(List<Nintendo3DsFontAtlasGlyphItem> items, int atlasWidth) {
        if (items == null) {
            throw new ArgumentNullException(nameof(items));
        }

        Dictionary<char, Point> positions = [];
        int currentX = 0;
        int currentY = 0;
        int rowHeight = 0;
        for (int itemIndex = 0; itemIndex < items.Count; itemIndex++) {
            Nintendo3DsFontAtlasGlyphItem item = items[itemIndex];
            int paddedWidth = item.Width + (AtlasPadding * 2);
            int paddedHeight = item.Height + (AtlasPadding * 2);
            if (currentX + paddedWidth > atlasWidth) {
                currentY += rowHeight;
                currentX = 0;
                rowHeight = 0;
            }

            positions[item.Character] = new Point(currentX, currentY);
            rowHeight = Math.Max(rowHeight, paddedHeight);
            currentX += paddedWidth;
        }

        return positions;
    }

    /// <summary>
    /// Renders the atlas bitmap from glyph bitmaps and calculated positions.
    /// </summary>
    /// <param name="items">Glyph items to draw.</param>
    /// <param name="positions">Positions of each glyph in the atlas.</param>
    /// <param name="width">Atlas width.</param>
    /// <param name="height">Atlas height.</param>
    /// <returns>Rendered atlas bitmap.</returns>
    static Bitmap CreateAtlasBitmap(List<Nintendo3DsFontAtlasGlyphItem> items, Dictionary<char, Point> positions, int width, int height) {
        if (items == null) {
            throw new ArgumentNullException(nameof(items));
        } else if (positions == null) {
            throw new ArgumentNullException(nameof(positions));
        }

        Bitmap atlas = new(width, height, PixelFormat.Format32bppArgb);
        using Graphics graphics = Graphics.FromImage(atlas);
        graphics.Clear(Color.Transparent);
        for (int itemIndex = 0; itemIndex < items.Count; itemIndex++) {
            Nintendo3DsFontAtlasGlyphItem item = items[itemIndex];
            Point position = positions[item.Character];
            graphics.DrawImage(
                item.Bitmap,
                position.X + AtlasPadding,
                position.Y + AtlasPadding,
                new Rectangle(item.SourceX, item.SourceY, item.Width, item.Height),
                GraphicsUnit.Pixel);
        }

        return atlas;
    }

    /// <summary>
    /// Converts one atlas bitmap into the raw RGBA texture asset payload expected by Nintendo 3DS texture cooking.
    /// </summary>
    /// <param name="atlas">Atlas bitmap to convert.</param>
    /// <returns>Raw RGBA texture asset.</returns>
    static TextureAsset ConvertAtlasBitmapToTextureAsset(Bitmap atlas) {
        if (atlas == null) {
            throw new ArgumentNullException(nameof(atlas));
        }

        Nintendo3DsBitmapLock bitmapLock = new(atlas);
        bitmapLock.LockBits();
        bitmapLock.UnlockBits(false);

        byte[] colors = bitmapLock.Pixels;
        for (int index = 0; index < colors.Length; index += 4) {
            byte alpha = colors[index];
            byte red = colors[index + 1];
            byte green = colors[index + 2];
            byte blue = colors[index + 3];

            colors[index] = red;
            colors[index + 1] = green;
            colors[index + 2] = blue;
            colors[index + 3] = alpha;
        }

        return new TextureAsset {
            Width = checked((ushort)atlas.Width),
            Height = checked((ushort)atlas.Height),
            Colors = colors,
            PaletteColors = Array.Empty<byte>(),
            ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = TextureAssetAlphaPrecision.A8
        };
    }
}



