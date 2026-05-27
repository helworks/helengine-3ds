using System.Drawing;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Represents one temporary glyph bitmap placement used during Nintendo 3DS source font atlas import.
/// </summary>
public sealed class Nintendo3DsFontAtlasGlyphItem {
    /// <summary>
    /// Gets or sets the character represented by this glyph item.
    /// </summary>
    public char Character { get; set; }

    /// <summary>
    /// Gets or sets the X position of the glyph within the source bitmap.
    /// </summary>
    public int SourceX { get; set; }

    /// <summary>
    /// Gets or sets the Y position of the glyph within the source bitmap.
    /// </summary>
    public int SourceY { get; set; }

    /// <summary>
    /// Gets or sets the width of the glyph bitmap.
    /// </summary>
    public int Width { get; set; }

    /// <summary>
    /// Gets or sets the height of the glyph bitmap.
    /// </summary>
    public int Height { get; set; }

    /// <summary>
    /// Gets or sets the bitmap image for the glyph.
    /// </summary>
    public Bitmap Bitmap { get; set; }
}

