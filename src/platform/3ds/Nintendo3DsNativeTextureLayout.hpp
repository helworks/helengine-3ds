#pragma once
#include <cstdint>

namespace helengine::nintendo3ds {
    /// Stores checked single-level N3T1 controls before allocating or uploading any GPU texture memory.
    struct Nintendo3DsNativeTextureLayout {
        /// Stores the GPU_TEXCOLOR encoding in the range zero through thirteen.
        std::uint32_t Format = 0;
        /// Stores the authored width used by sprite and model UV calculations.
        std::uint32_t Width = 0;
        /// Stores the authored height used by sprite and font rectangles.
        std::uint32_t Height = 0;
        /// Stores the padded power-of-two GPU width.
        std::uint32_t StorageWidth = 0;
        /// Stores the padded power-of-two GPU height.
        std::uint32_t StorageHeight = 0;
        /// Stores the exact tiled or ETC texel byte count.
        std::uint32_t TexelBytes = 0;
    };
}
