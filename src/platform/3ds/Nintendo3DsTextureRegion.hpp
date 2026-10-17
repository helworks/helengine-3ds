#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace helengine::nintendo3ds {
    /// Validates the complete source and destination region before any GPU synchronization or texture mutation.
    inline void ValidateTextureRegion(int32_t textureWidth, int32_t textureHeight, int32_t x, int32_t y,
        int32_t width, int32_t height, const uint8_t* source, int32_t sourceLength, int32_t rowPitch) {
        if (source == nullptr || x < 0 || y < 0 || width <= 0 || height <= 0
            || static_cast<int64_t>(x) + width > textureWidth || static_cast<int64_t>(y) + height > textureHeight
            || static_cast<int64_t>(width) * 4 > rowPitch || rowPitch % 4 != 0
            || static_cast<int64_t>(height - 1) * rowPitch + static_cast<int64_t>(width) * 4 > sourceLength) {
            throw std::invalid_argument("Nintendo 3DS RGBA8 texture region has invalid bounds, stride or source storage.");
        }
    }

    /// Copies a previously validated RGBA8 region into PICA 8x8 Morton tiles, preserving texels outside the region.
    inline void WriteTextureRegion(uint8_t* destination, int32_t storageWidth, int32_t x, int32_t y,
        int32_t width, int32_t height, const uint8_t* source, int32_t rowPitch) {
        for (int32_t row = 0; row < height; row++) {
            for (int32_t column = 0; column < width; column++) {
                const uint32_t destinationX = static_cast<uint32_t>(x + column);
                const uint32_t destinationY = static_cast<uint32_t>(y + row);
                uint32_t morton = 0;
                for (uint32_t bit = 0; bit < 3; bit++) {
                    morton |= ((destinationX >> bit) & 1U) << (bit * 2);
                    morton |= ((destinationY >> bit) & 1U) << (bit * 2 + 1);
                }
                const std::size_t texel = ((destinationY / 8) * (storageWidth / 8) + destinationX / 8) * 64 + morton;
                const uint8_t* rgba = source + static_cast<std::size_t>(row) * rowPitch + column * 4;
                uint8_t* abgr = destination + texel * 4;
                abgr[0] = rgba[3]; abgr[1] = rgba[2]; abgr[2] = rgba[1]; abgr[3] = rgba[0];
            }
        }
    }
}
