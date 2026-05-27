using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Provides one helper for locking and editing bitmap pixel data safely during Nintendo 3DS source font atlas generation.
/// </summary>
[SupportedOSPlatform("windows")]
public sealed class Nintendo3DsBitmapLock {
    /// <summary>
    /// Stores the source bitmap currently being locked.
    /// </summary>
    readonly Bitmap Source;

    /// <summary>
    /// Stores the native pointer returned by <see cref="Bitmap.LockBits(Rectangle, ImageLockMode, PixelFormat)"/>.
    /// </summary>
    IntPtr PixelsPointer = IntPtr.Zero;

    /// <summary>
    /// Stores the active locked bitmap metadata.
    /// </summary>
    BitmapData BitmapData;

    /// <summary>
    /// Initializes one bitmap lock helper for the supplied bitmap.
    /// </summary>
    /// <param name="source">Bitmap to lock for pixel access.</param>
    public Nintendo3DsBitmapLock(Bitmap source) {
        Source = source ?? throw new ArgumentNullException(nameof(source));
    }

    /// <summary>
    /// Gets or sets the pixel buffer representing the locked bitmap.
    /// </summary>
    public byte[] Pixels { get; set; }

    /// <summary>
    /// Gets the pixel depth in bits for the locked bitmap.
    /// </summary>
    public int Depth { get; private set; }

    /// <summary>
    /// Gets the width of the locked bitmap.
    /// </summary>
    public int Width { get; private set; }

    /// <summary>
    /// Gets the height of the locked bitmap.
    /// </summary>
    public int Height { get; private set; }

    /// <summary>
    /// Locks the bitmap for read/write access and fills the <see cref="Pixels"/> buffer.
    /// </summary>
    public void LockBits() {
        Width = Source.Width;
        Height = Source.Height;

        int pixelCount = Width * Height;
        Rectangle rectangle = new(0, 0, Width, Height);
        Depth = Bitmap.GetPixelFormatSize(Source.PixelFormat);
        if (Depth != 8 && Depth != 24 && Depth != 32) {
            throw new ArgumentException("Only 8, 24 and 32 bpp images are supported.");
        }

        BitmapData = Source.LockBits(rectangle, ImageLockMode.ReadWrite, Source.PixelFormat);
        int step = Depth / 8;
        Pixels = new byte[pixelCount * step];
        PixelsPointer = BitmapData.Scan0;
        Marshal.Copy(PixelsPointer, Pixels, 0, Pixels.Length);
    }

    /// <summary>
    /// Unlocks the bitmap data and optionally writes modified pixels back to the source bitmap.
    /// </summary>
    /// <param name="transferBack">True when the <see cref="Pixels"/> buffer should be copied back before unlocking.</param>
    public void UnlockBits(bool transferBack) {
        if (transferBack) {
            Marshal.Copy(Pixels, 0, PixelsPointer, Pixels.Length);
        }

        Source.UnlockBits(BitmapData);
    }

    /// <summary>
    /// Gets the color of the specified pixel.
    /// </summary>
    /// <param name="x">X coordinate of the pixel.</param>
    /// <param name="y">Y coordinate of the pixel.</param>
    /// <returns>Color value at the specified coordinate.</returns>
    public Color GetPixel(int x, int y) {
        int componentCount = Depth / 8;
        int pixelIndex = ((y * Width) + x) * componentCount;
        if (pixelIndex > Pixels.Length - componentCount) {
            throw new IndexOutOfRangeException();
        }

        if (Depth == 32) {
            byte blue = Pixels[pixelIndex];
            byte green = Pixels[pixelIndex + 1];
            byte red = Pixels[pixelIndex + 2];
            byte alpha = Pixels[pixelIndex + 3];
            return Color.FromArgb(alpha, red, green, blue);
        } else if (Depth == 24) {
            byte blue = Pixels[pixelIndex];
            byte green = Pixels[pixelIndex + 1];
            byte red = Pixels[pixelIndex + 2];
            return Color.FromArgb(red, green, blue);
        }

        byte component = Pixels[pixelIndex];
        return Color.FromArgb(component, component, component);
    }
}

