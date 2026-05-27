using System.Drawing;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Represents one temporary glyph definition produced during Nintendo 3DS source font atlas import.
/// </summary>
public readonly struct Nintendo3DsFontAtlasGlyph {
    /// <summary>
    /// Initializes one temporary glyph definition.
    /// </summary>
    /// <param name="sourceRect">Source rectangle for the glyph.</param>
    /// <param name="bitmap">Bitmap containing the glyph pixels.</param>
    public Nintendo3DsFontAtlasGlyph(int4 sourceRect, Bitmap bitmap) {
        SourceRect = sourceRect;
        Bitmap = bitmap ?? throw new ArgumentNullException(nameof(bitmap));
    }

    /// <summary>
    /// Gets the source rectangle for the glyph within its temporary bitmap.
    /// </summary>
    public int4 SourceRect { get; }

    /// <summary>
    /// Gets the bitmap containing the glyph pixels.
    /// </summary>
    public Bitmap Bitmap { get; }
}

