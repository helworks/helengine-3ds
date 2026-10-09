#include "platform/3ds/Nintendo3DsNativeTexture.hpp"
#include <stdexcept>

namespace helengine::nintendo3ds {
    namespace {
        /// Reads a little-endian descriptor word after the fixed header has been bounded.
        std::uint32_t Word(const std::uint8_t* data, int offset) {
            return std::uint32_t(data[offset]) | std::uint32_t(data[offset + 1]) << 8 | std::uint32_t(data[offset + 2]) << 16 | std::uint32_t(data[offset + 3]) << 24;
        }
        /// Rounds one authored extent to a supported GPU power of two with a minimum of eight.
        std::uint32_t PowerOfTwo(std::uint32_t value) {
            std::uint32_t result = 8;
            while (result < value) result *= 2;
            return result;
        }
        /// Gets component or average compressed bits per texel for a checked PICA encoding.
        std::uint32_t Bits(std::uint32_t code) {
            return code == 0 ? 32 : code == 1 ? 24 : code >= 2 && code <= 6 ? 16 : code == 10 || code == 11 || code == 12 ? 4 : 8;
        }
    }
    /// Rejects malformed descriptors before their format or byte ranges are submitted to citro3d.
    Nintendo3DsNativeTextureLayout Nintendo3DsNativeTexture::Read(const std::uint8_t* colors, std::size_t colorBytes, std::size_t paletteBytes, std::uint32_t width, std::uint32_t height) {
        if (colors == nullptr || colorBytes < 32 || Word(colors, 0) != 0x3154334eu || Word(colors, 4) != 1 || paletteBytes != 0
            || width < 1 || height < 1 || width > 1024 || height > 1024 || Word(colors, 8) > 13) throw std::invalid_argument("Invalid Nintendo 3DS N3T1 descriptor.");
        Nintendo3DsNativeTextureLayout layout;
        layout.Format = Word(colors, 8); layout.Width = width; layout.Height = height;
        layout.StorageWidth = PowerOfTwo(width); layout.StorageHeight = PowerOfTwo(height);
        layout.TexelBytes = layout.StorageWidth * layout.StorageHeight * Bits(layout.Format) / 8;
        if (Word(colors, 12) != width || Word(colors, 16) != height || Word(colors, 20) != layout.StorageWidth
            || Word(colors, 24) != layout.StorageHeight || Word(colors, 28) != layout.TexelBytes || colorBytes != 32 + layout.TexelBytes) throw std::invalid_argument("Malformed Nintendo 3DS native texture bounds.");
        return layout;
    }
}
