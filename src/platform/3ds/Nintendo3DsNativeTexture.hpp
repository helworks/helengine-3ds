#pragma once
#include <cstddef>
#include <cstdint>
#include "platform/3ds/Nintendo3DsNativeTextureLayout.hpp"

namespace helengine::nintendo3ds {
    /// Validates GPU-ready PICA textures without expanding component or compressed storage.
    class Nintendo3DsNativeTexture final {
    public:
        /// Checks the entire N3T1 descriptor, canonical storage dimensions, palette absence and exact payload length.
        static Nintendo3DsNativeTextureLayout Read(const std::uint8_t* colors, std::size_t colorBytes, std::size_t paletteBytes, std::uint32_t width, std::uint32_t height);
    };
}
