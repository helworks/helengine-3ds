#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <string>

#include "RuntimeMaterial.hpp"
#include "float4.hpp"

namespace helengine::nintendo3ds {
    class Nintendo3DsRuntimeTexture;

    /// Carries the minimal Nintendo 3DS-owned material state required by the lit 3D material shader paths.
    class Nintendo3DsRuntimeMaterial final : public RuntimeMaterial {
    public:
        /// Creates one Nintendo 3DS runtime material with a visible opaque white base color.
        Nintendo3DsRuntimeMaterial();

        /// Releases any diffuse texture upload owned directly by this runtime material.
        ~Nintendo3DsRuntimeMaterial() override;

        /// Gets the normalized authored base color resolved from the cooked platform material payload.
        float4 GetBaseColor() const;

        /// Replaces the normalized authored base color used by the Nintendo 3DS material shader path.
        void SetBaseColor(float4 value);

        /// Gets the cooked diffuse-texture path resolved from the packaged platform material payload.
        const std::string& GetTextureRelativePath() const;

        /// Replaces the cooked diffuse-texture path used to rebuild the Nintendo 3DS runtime texture.
        void SetTextureRelativePath(std::string value);

        /// Gets the Nintendo 3DS runtime diffuse texture owned directly by this material when the cooked-material contract loaded one internally.
        Nintendo3DsRuntimeTexture* GetOwnedDiffuseTexture() const;

        /// Replaces the Nintendo 3DS runtime diffuse texture owned directly by this material when the cooked-material contract loaded one internally.
        void SetOwnedDiffuseTexture(Nintendo3DsRuntimeTexture* value);

    private:
        /// Stores the normalized authored RGBA base color copied from the cooked platform material payload.
        float4 BaseColorValue;

        /// Stores the cooked diffuse-texture path copied from the packaged platform material payload.
        std::string TextureRelativePathValue;

        /// Stores the Nintendo 3DS runtime diffuse texture owned directly by this material when it was loaded through the path-based cooked-material contract.
        Nintendo3DsRuntimeTexture* OwnedDiffuseTextureValue;
    };
}

#endif
