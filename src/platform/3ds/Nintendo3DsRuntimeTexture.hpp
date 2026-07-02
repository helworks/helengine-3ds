#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <tex3ds.h>

#include <string>

#include "RuntimeTexture.hpp"

class TextureAsset;

namespace helengine::nintendo3ds {
    /// Stores one Nintendo 3DS-native citro3d texture together with the metadata needed to build citro2d images from authored atlas source rectangles.
    class Nintendo3DsRuntimeTexture final : public RuntimeTexture {
    public:
        /// Creates one empty Nintendo 3DS runtime texture with no native texture upload yet.
        Nintendo3DsRuntimeTexture();

        /// Releases one owned Nintendo 3DS native texture upload.
        ~Nintendo3DsRuntimeTexture() override;

        /// Uploads one shared-engine texture asset into the Nintendo 3DS tiled GPU texture layout.
        /// <param name="data">Shared-engine texture asset to upload.</param>
        void LoadFromRaw(TextureAsset* data);

        /// Returns whether one native citro3d texture upload exists for this runtime texture.
        bool HasNativeTexture() const;

        /// Returns the native citro3d texture used by citro2d sprite drawing.
        C3D_Tex* GetNativeTexture();

        /// Gets the actual authored texture width before power-of-two padding.
        uint16_t GetActualWidth() const;

        /// Gets the actual authored texture height before power-of-two padding.
        uint16_t GetActualHeight() const;

        /// Gets the padded native texture width used by the Nintendo 3DS GPU upload.
        uint16_t GetPaddedWidth() const;

        /// Gets the padded native texture height used by the Nintendo 3DS GPU upload.
        uint16_t GetPaddedHeight() const;

        /// Returns one deferred diagnostic summary describing representative cooked texels for this runtime texture.
        const std::string& GetDebugTraceSummary() const;

        /// Returns the deferred diagnostic summary once so later render-time tracing can show what texels were uploaded.
        /// <param name="summary">Receives the deferred diagnostic summary when available.</param>
        /// <returns>True when a deferred diagnostic summary was returned.</returns>
        bool TryTakeDebugTraceSummary(std::string& summary);

    private:
        /// Stores the actual authored texture width before GPU padding.
        uint16_t ActualWidth;

        /// Stores the actual authored texture height before GPU padding.
        uint16_t ActualHeight;

        /// Stores the padded power-of-two width used by the native GPU texture.
        uint16_t PaddedWidth;

        /// Stores the padded power-of-two height used by the native GPU texture.
        uint16_t PaddedHeight;

        /// Stores the native citro3d texture object.
        C3D_Tex NativeTexture;

        /// Stores whether the native citro3d texture object currently owns uploaded texture memory.
        bool NativeTextureInitialized;

        /// Stores one deferred diagnostic summary describing representative cooked texels for this runtime texture.
        std::string DebugTraceSummary;

        /// Stores whether the deferred diagnostic summary has already been consumed by render-time tracing.
        bool DebugTraceSummaryEmitted;

        /// Releases any currently owned Nintendo 3DS native texture upload.
        void Reset();
    };
}

#endif
