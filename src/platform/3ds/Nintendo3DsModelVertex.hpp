#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

namespace helengine::nintendo3ds {
    /// Stores one expanded Nintendo 3DS triangle vertex with authored position, texture-coordinate, and normal data interleaved for shader submission.
    struct Nintendo3DsModelVertex final {
        /// Stores the authored model-space position consumed by the Nintendo 3DS vertex shader.
        float PositionX;

        /// Stores the authored model-space position Y component consumed by the Nintendo 3DS vertex shader.
        float PositionY;

        /// Stores the authored model-space position Z component consumed by the Nintendo 3DS vertex shader.
        float PositionZ;

        /// Stores the authored model-space texture coordinate consumed by the textured Nintendo 3DS shader path.
        float TextureCoordinateX;

        /// Stores the authored model-space texture-coordinate Y component consumed by the textured Nintendo 3DS shader path.
        float TextureCoordinateY;

        /// Stores the authored model-space normal consumed by the Nintendo 3DS lighting shader.
        float NormalX;

        /// Stores the authored model-space normal Y component consumed by the Nintendo 3DS lighting shader.
        float NormalY;

        /// Stores the authored model-space normal Z component consumed by the Nintendo 3DS lighting shader.
        float NormalZ;
    };
}

#endif
