#include "platform/3ds/Nintendo3DsRuntimeMaterial.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <utility>

#include "platform/3ds/Nintendo3DsRuntimeTexture.hpp"

namespace helengine::nintendo3ds {
    /// Creates one Nintendo 3DS runtime material with a visible opaque white base color.
    Nintendo3DsRuntimeMaterial::Nintendo3DsRuntimeMaterial()
        : RuntimeMaterial()
        , BaseColorValue(1.0f, 1.0f, 1.0f, 1.0f)
        , TextureRelativePathValue()
        , OwnedDiffuseTextureValue(nullptr) {
    }

    /// Releases any diffuse texture upload owned directly by this runtime material.
    Nintendo3DsRuntimeMaterial::~Nintendo3DsRuntimeMaterial() {
        SetOwnedDiffuseTexture(nullptr);
    }

    /// Gets the normalized authored base color resolved from the cooked platform material payload.
    float4 Nintendo3DsRuntimeMaterial::GetBaseColor() const {
        return BaseColorValue;
    }

    /// Replaces the normalized authored base color used by the Nintendo 3DS material shader path.
    void Nintendo3DsRuntimeMaterial::SetBaseColor(float4 value) {
        BaseColorValue = value;
    }

    /// Gets the cooked diffuse-texture path resolved from the packaged platform material payload.
    const std::string& Nintendo3DsRuntimeMaterial::GetTextureRelativePath() const {
        return TextureRelativePathValue;
    }

    /// Replaces the cooked diffuse-texture path used to rebuild the Nintendo 3DS runtime texture.
    void Nintendo3DsRuntimeMaterial::SetTextureRelativePath(std::string value) {
        TextureRelativePathValue = std::move(value);
    }

    /// Gets the Nintendo 3DS runtime diffuse texture owned directly by this material when the cooked-material contract loaded one internally.
    Nintendo3DsRuntimeTexture* Nintendo3DsRuntimeMaterial::GetOwnedDiffuseTexture() const {
        return OwnedDiffuseTextureValue;
    }

    /// Replaces the Nintendo 3DS runtime diffuse texture owned directly by this material when the cooked-material contract loaded one internally.
    void Nintendo3DsRuntimeMaterial::SetOwnedDiffuseTexture(Nintendo3DsRuntimeTexture* value) {
        if (OwnedDiffuseTextureValue == value) {
            return;
        }

        if (OwnedDiffuseTextureValue != nullptr) {
            delete OwnedDiffuseTextureValue;
        }

        OwnedDiffuseTextureValue = value;
    }
}

#endif
