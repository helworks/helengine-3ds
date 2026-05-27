#include "platform/3ds/Nintendo3DsRuntimeTexture.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "TextureAsset.hpp"
#include "TextureAssetAlphaPrecision.hpp"
#include "TextureAssetColorFormat.hpp"
#include "byte4.hpp"
#include "runtime/array.hpp"

namespace helengine::nintendo3ds {
    namespace {
        /// Stores the number of bytes per RGBA32 texel in authored runtime texture payloads.
        constexpr int32_t Rgba32BytesPerPixel = 4;

        /// Stores the number of bytes per RGBA4444 texel in authored runtime texture payloads.
        constexpr int32_t Rgba4444BytesPerPixel = 2;

        /// Stores the number of bytes used by one indexed-8 texel.
        constexpr int32_t Indexed8BytesPerPixel = 1;

        /// Stores the number of RGBA bytes carried by one palette entry.
        constexpr int32_t PaletteEntryBytes = 4;

        /// Converts one 8-bit channel pair into the modulation result used by shared-engine sprite tinting.
        uint8_t ExpandFourBitChannel(uint16_t packedColor, int32_t shift) {
            uint8_t channel = static_cast<uint8_t>((packedColor >> shift) & 15);
            return static_cast<uint8_t>((channel << 4) | channel);
        }

        /// Decodes one packed RGBA4444 texel into one shared byte4 color.
        byte4 UnpackRgba4444(uint16_t packedColor) {
            return byte4(
                ExpandFourBitChannel(packedColor, 0),
                ExpandFourBitChannel(packedColor, 4),
                ExpandFourBitChannel(packedColor, 8),
                ExpandFourBitChannel(packedColor, 12));
        }

        /// Reorders one shared-engine RGBA4444 texel into the little-endian ABGR nibble layout consumed by the Nintendo 3DS RGBA4 upload path.
        uint16_t PackNativeRgba4(uint16_t packedColor) {
            uint16_t red = static_cast<uint16_t>((packedColor >> 0) & 15);
            uint16_t green = static_cast<uint16_t>((packedColor >> 4) & 15);
            uint16_t blue = static_cast<uint16_t>((packedColor >> 8) & 15);
            uint16_t alpha = static_cast<uint16_t>((packedColor >> 12) & 15);
            return static_cast<uint16_t>(alpha | (blue << 4) | (green << 8) | (red << 12));
        }

        /// Resolves the expected cooked payload length for one texture asset based on its serialized texture format.
        int32_t GetExpectedColorLength(TextureAssetColorFormat colorFormat, int32_t width, int32_t height) {
            if (colorFormat == TextureAssetColorFormat::Rgba32) {
                return width * height * Rgba32BytesPerPixel;
            } else if (colorFormat == TextureAssetColorFormat::Rgba4444) {
                return width * height * Rgba4444BytesPerPixel;
            } else if (colorFormat == TextureAssetColorFormat::Indexed4) {
                return ((width * height) + 1) / 2;
            } else if (colorFormat == TextureAssetColorFormat::Indexed8) {
                return width * height * Indexed8BytesPerPixel;
            }

            throw std::invalid_argument("Nintendo 3DS runtime texture received one unsupported texture color format.");
        }

        /// Resolves the maximum supported cooked palette payload length for one indexed texture format.
        int32_t GetMaximumPaletteColorLength(TextureAssetColorFormat colorFormat) {
            if (colorFormat == TextureAssetColorFormat::Indexed4) {
                return 16 * PaletteEntryBytes;
            } else if (colorFormat == TextureAssetColorFormat::Indexed8) {
                return 256 * PaletteEntryBytes;
            }

            return 0;
        }

        /// Reads one packed 4-bit palette index from one indexed-4 texture payload.
        uint8_t ReadPackedNibbleIndex(Array<uint8_t>* colors, int32_t textureWidth, int32_t sampleX, int32_t sampleY) {
            int32_t pixelIndex = (sampleY * textureWidth) + sampleX;
            int32_t sourceIndex = pixelIndex / 2;
            uint8_t packedIndices = colors->Data[sourceIndex];
            if ((pixelIndex & 1) == 0) {
                return static_cast<uint8_t>(packedIndices & 0x0F);
            }

            return static_cast<uint8_t>((packedIndices >> 4) & 0x0F);
        }

        /// Rounds one authored texture dimension up to the next power of two required by the Nintendo 3DS GPU texture layout.
        uint16_t RoundUpToPowerOfTwo(uint16_t value) {
            if (value == 0) {
                throw std::invalid_argument("Nintendo 3DS runtime textures require one nonzero dimension.");
            }

            uint32_t roundedValue = 1;
            while (roundedValue < value) {
                roundedValue <<= 1;
            }

            if (roundedValue > 1024U) {
                throw std::invalid_argument("Nintendo 3DS runtime textures currently support dimensions up to 1024 pixels.");
            }

            return static_cast<uint16_t>(roundedValue);
        }

        /// Resolves the native Nintendo 3DS GPU texture format that should back one cooked texture payload.
        GPU_TEXCOLOR ResolveNativeTextureFormat(TextureAssetColorFormat colorFormat) {
            if (colorFormat == TextureAssetColorFormat::Rgba4444) {
                return GPU_RGBA4;
            }

            return GPU_RGBA8;
        }

        /// Resolves the number of staging bytes needed for one native Nintendo 3DS texel.
        int32_t GetNativeBytesPerPixel(GPU_TEXCOLOR nativeTextureFormat) {
            if (nativeTextureFormat == GPU_RGBA4) {
                return Rgba4444BytesPerPixel;
            }

            return Rgba32BytesPerPixel;
        }

        /// Resolves the display-transfer flags used to convert one linear upload buffer into the Nintendo 3DS tiled texture layout.
        u32 ResolveTextureTransferFlags(GPU_TEXCOLOR nativeTextureFormat) {
            if (nativeTextureFormat == GPU_RGBA4) {
                return GX_TRANSFER_FLIP_VERT(0)
                    | GX_TRANSFER_OUT_TILED(1)
                    | GX_TRANSFER_RAW_COPY(0)
                    | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA4)
                    | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA4)
                    | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
            }

            return GX_TRANSFER_FLIP_VERT(0)
                | GX_TRANSFER_OUT_TILED(1)
                | GX_TRANSFER_RAW_COPY(0)
                | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)
                | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8)
                | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
        }
    }

    /// Creates one empty Nintendo 3DS runtime texture with no native texture upload yet.
    Nintendo3DsRuntimeTexture::Nintendo3DsRuntimeTexture()
        : RuntimeTexture()
        , ActualWidth(0)
        , ActualHeight(0)
        , PaddedWidth(0)
        , PaddedHeight(0)
        , NativeTexture {}
        , NativeTextureInitialized(false) {
    }

    /// Releases one owned Nintendo 3DS native texture upload.
    Nintendo3DsRuntimeTexture::~Nintendo3DsRuntimeTexture() {
        Reset();
    }

    /// Uploads one shared-engine texture asset into the Nintendo 3DS tiled GPU texture layout.
    void Nintendo3DsRuntimeTexture::LoadFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("Nintendo 3DS runtime texture upload requires one texture asset.");
        } else if (data->Width == 0 || data->Height == 0) {
            throw std::invalid_argument("Nintendo 3DS runtime textures require nonzero dimensions.");
        } else if (data->Colors == nullptr || data->Colors->Data == nullptr) {
            throw std::invalid_argument("Nintendo 3DS runtime textures require one color payload.");
        }

        int32_t textureWidth = static_cast<int32_t>(data->Width);
        int32_t textureHeight = static_cast<int32_t>(data->Height);
        int32_t expectedColorLength = GetExpectedColorLength(data->ColorFormat, textureWidth, textureHeight);
        if (data->Colors->Length != expectedColorLength) {
            throw std::invalid_argument("Nintendo 3DS runtime textures require one cooked color payload with the expected byte length.");
        }

        if (data->ColorFormat == TextureAssetColorFormat::Indexed4 || data->ColorFormat == TextureAssetColorFormat::Indexed8) {
            if (data->PaletteColors == nullptr || data->PaletteColors->Data == nullptr) {
                throw std::invalid_argument("Nintendo 3DS indexed runtime textures require one palette payload.");
            }

            int32_t maximumPaletteColorLength = GetMaximumPaletteColorLength(data->ColorFormat);
            if (data->PaletteColors->Length < PaletteEntryBytes
                || data->PaletteColors->Length > maximumPaletteColorLength
                || (data->PaletteColors->Length % PaletteEntryBytes) != 0) {
                throw std::invalid_argument("Nintendo 3DS indexed runtime textures require one RGBA palette payload with a supported byte length.");
            }
        }

        Reset();

        ActualWidth = data->Width;
        ActualHeight = data->Height;
        PaddedWidth = RoundUpToPowerOfTwo(ActualWidth);
        PaddedHeight = RoundUpToPowerOfTwo(ActualHeight);
        GPU_TEXCOLOR nativeTextureFormat = ResolveNativeTextureFormat(data->ColorFormat);

        if (!C3D_TexInit(&NativeTexture, PaddedWidth, PaddedHeight, nativeTextureFormat)) {
            throw std::runtime_error("Nintendo 3DS runtime texture upload failed to initialize the native GPU texture.");
        }

        NativeTextureInitialized = true;
        C3D_TexSetFilter(&NativeTexture, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&NativeTexture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

        std::size_t paddedPixelCount = static_cast<std::size_t>(PaddedWidth) * static_cast<std::size_t>(PaddedHeight);
        std::size_t paddedByteCount = paddedPixelCount * static_cast<std::size_t>(GetNativeBytesPerPixel(nativeTextureFormat));
        uint8_t* uploadBuffer = static_cast<uint8_t*>(linearAlloc(paddedByteCount));
        if (uploadBuffer == nullptr) {
            Reset();
            throw std::runtime_error("Nintendo 3DS runtime texture upload failed to allocate one linear staging buffer.");
        }

        std::memset(uploadBuffer, 0, paddedByteCount);

        try {
            for (int32_t y = 0; y < textureHeight; y++) {
                for (int32_t x = 0; x < textureWidth; x++) {
                    std::size_t destinationIndex = (static_cast<std::size_t>(y) * static_cast<std::size_t>(PaddedWidth) + static_cast<std::size_t>(x))
                        * static_cast<std::size_t>(GetNativeBytesPerPixel(nativeTextureFormat));
                    if (nativeTextureFormat == GPU_RGBA4) {
                        int32_t sourceIndex = ((y * textureWidth) + x) * Rgba4444BytesPerPixel;
                        uint16_t packedColor = static_cast<uint16_t>(data->Colors->Data[sourceIndex] | (data->Colors->Data[sourceIndex + 1] << 8));
                        uint16_t nativePackedColor = PackNativeRgba4(packedColor);
                        uploadBuffer[destinationIndex] = static_cast<uint8_t>(nativePackedColor & 0xFF);
                        uploadBuffer[destinationIndex + 1] = static_cast<uint8_t>((nativePackedColor >> 8) & 0xFF);
                        continue;
                    }

                    byte4 decodedColor;
                    if (data->ColorFormat == TextureAssetColorFormat::Rgba32) {
                        int32_t sourceIndex = ((y * textureWidth) + x) * Rgba32BytesPerPixel;
                        decodedColor = byte4(
                            data->Colors->Data[sourceIndex],
                            data->Colors->Data[sourceIndex + 1],
                            data->Colors->Data[sourceIndex + 2],
                            data->Colors->Data[sourceIndex + 3]);
                    } else if (data->ColorFormat == TextureAssetColorFormat::Rgba4444) {
                        int32_t sourceIndex = ((y * textureWidth) + x) * Rgba4444BytesPerPixel;
                        uint16_t packedColor = static_cast<uint16_t>(data->Colors->Data[sourceIndex] | (data->Colors->Data[sourceIndex + 1] << 8));
                        decodedColor = UnpackRgba4444(packedColor);
                    } else {
                        uint8_t paletteIndex = data->ColorFormat == TextureAssetColorFormat::Indexed4
                            ? ReadPackedNibbleIndex(data->Colors, textureWidth, x, y)
                            : data->Colors->Data[(y * textureWidth) + x];
                        int32_t paletteOffset = static_cast<int32_t>(paletteIndex) * PaletteEntryBytes;
                        if (paletteOffset < 0 || paletteOffset + 3 >= data->PaletteColors->Length) {
                            throw std::invalid_argument("Nintendo 3DS indexed runtime texture palette index exceeded the cooked palette payload.");
                        }

                        decodedColor = byte4(
                            data->PaletteColors->Data[paletteOffset],
                            data->PaletteColors->Data[paletteOffset + 1],
                            data->PaletteColors->Data[paletteOffset + 2],
                            data->PaletteColors->Data[paletteOffset + 3]);
                    }

                    uploadBuffer[destinationIndex] = decodedColor.W;
                    uploadBuffer[destinationIndex + 1] = decodedColor.Z;
                    uploadBuffer[destinationIndex + 2] = decodedColor.Y;
                    uploadBuffer[destinationIndex + 3] = decodedColor.X;
                }
            }

            GSPGPU_FlushDataCache(uploadBuffer, paddedByteCount);
            C3D_SyncDisplayTransfer(
                reinterpret_cast<u32*>(uploadBuffer),
                GX_BUFFER_DIM(PaddedWidth, PaddedHeight),
                reinterpret_cast<u32*>(NativeTexture.data),
                GX_BUFFER_DIM(PaddedWidth, PaddedHeight),
                ResolveTextureTransferFlags(nativeTextureFormat));
        } catch (...) {
            linearFree(uploadBuffer);
            Reset();
            throw;
        }

        linearFree(uploadBuffer);
        C3D_TexFlush(&NativeTexture);
        set_Width(ActualWidth);
        set_Height(ActualHeight);
    }

    /// Returns whether one native citro3d texture upload exists for this runtime texture.
    bool Nintendo3DsRuntimeTexture::HasNativeTexture() const {
        return NativeTextureInitialized;
    }

    /// Returns the native citro3d texture used by citro2d sprite drawing.
    C3D_Tex* Nintendo3DsRuntimeTexture::GetNativeTexture() {
        if (!NativeTextureInitialized) {
            throw std::runtime_error("Nintendo 3DS runtime texture does not own one initialized native GPU texture.");
        }

        return &NativeTexture;
    }

    /// Gets the actual authored texture width before power-of-two padding.
    uint16_t Nintendo3DsRuntimeTexture::GetActualWidth() const {
        return ActualWidth;
    }

    /// Gets the actual authored texture height before power-of-two padding.
    uint16_t Nintendo3DsRuntimeTexture::GetActualHeight() const {
        return ActualHeight;
    }

    /// Gets the padded native texture width used by the Nintendo 3DS GPU upload.
    uint16_t Nintendo3DsRuntimeTexture::GetPaddedWidth() const {
        return PaddedWidth;
    }

    /// Gets the padded native texture height used by the Nintendo 3DS GPU upload.
    uint16_t Nintendo3DsRuntimeTexture::GetPaddedHeight() const {
        return PaddedHeight;
    }

    /// Releases any currently owned Nintendo 3DS native texture upload.
    void Nintendo3DsRuntimeTexture::Reset() {
        if (NativeTextureInitialized) {
            C3D_TexDelete(&NativeTexture);
            std::memset(&NativeTexture, 0, sizeof(NativeTexture));
            NativeTextureInitialized = false;
        }

        ActualWidth = 0;
        ActualHeight = 0;
        PaddedWidth = 0;
        PaddedHeight = 0;
    }
}

#endif
