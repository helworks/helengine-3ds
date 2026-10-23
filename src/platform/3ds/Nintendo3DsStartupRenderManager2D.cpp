#include "platform/3ds/Nintendo3DsStartupRenderManager2D.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Asset.hpp"
#include "Batch2DGeometry.hpp"
#include "platform/3ds/Nintendo3DsSpriteGeometry.hpp"
#include "AssetSerializer.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "FontAsset.hpp"
#include "FontChar.hpp"
#include "FontInfo.hpp"
#include "IContentStreamSource.hpp"
#include "IDrawable2D.hpp"
#include "ICamera.hpp"
#include "IRenderQueue2D.hpp"
#include "IRoundedRectDrawable2D.hpp"
#include "ISpriteDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "ObjectManager.hpp"
#include "TextLayoutUtils.hpp"
#include "TextureAsset.hpp"
#include "byte4.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "int2.hpp"
#include "CameraClearSettings.hpp"
#include "platform/3ds/Nintendo3DsRuntimeTexture.hpp"
#include "platform/3ds/Nintendo3DsTextureRegion.hpp"
#include "runtime/native_cast.hpp"
#include "runtime/array.hpp"
#include "system/io/file.hpp"

namespace helengine::nintendo3ds {
    namespace {
#ifndef HELENGINE_3DS_RENDER_TRACE_ENABLED
#define HELENGINE_3DS_RENDER_TRACE_ENABLED 0
#endif

        /// Stores the Nintendo 3DS top-screen width in pixels.
        constexpr int32_t Nintendo3DsTopScreenWidth = 400;

        /// Stores the Nintendo 3DS top-screen height in pixels.
        constexpr int32_t Nintendo3DsTopScreenHeight = 240;

        /// Stores the Nintendo 3DS bottom-screen width in pixels.
        constexpr int32_t Nintendo3DsBottomScreenWidth = 320;

        /// Stores the Nintendo 3DS bottom-screen height in pixels.
        constexpr int32_t Nintendo3DsBottomScreenHeight = 240;

        /// Stores the shared SD-card trace path used by Nintendo 3DS renderer diagnostics.
        constexpr const char* Nintendo3DsRenderTracePath = "sdmc:/helengine_3ds_render_trace.txt";

        /// Stores how many 2D frame trace samples remain before the renderer stops appending diagnostics.
        int Nintendo3DsRender2DTraceFramesRemaining = 16;

        /// Stores how many detailed 2D draw-call trace lines remain before per-draw diagnostics stop appending.
        int Nintendo3DsRender2DDetailLinesRemaining = 24;

        /// Appends one 2D capture boundary marker to the boot host's present trace.
        /// <param name="marker">Stable 2D capture boundary name.</param>
        void Append2DStageMarker(const char* marker) {
            static_cast<void>(marker);
        }

        /// Appends one diagnostic line to the shared Nintendo 3DS renderer trace file.
        /// <param name="message">Trace line that describes one 2D renderer boundary.</param>
        void AppendRenderTrace(const char* message) {
#if HELENGINE_3DS_RENDER_TRACE_ENABLED
            if (message == nullptr || Nintendo3DsRender2DTraceFramesRemaining <= 0) {
                return;
            }

            std::FILE* file = std::fopen(Nintendo3DsRenderTracePath, "a");
            if (file == nullptr) {
                return;
            }

            std::fprintf(file, "t=%llu %s\n", static_cast<unsigned long long>(osGetTime()), message);
            std::fclose(file);
#else
            static_cast<void>(message);
#endif
        }

        /// Appends one detailed draw-call diagnostic line while the detail budget remains available.
        /// <param name="message">Detailed draw-call trace payload.</param>
        void AppendDetailedRenderTrace(const char* message) {
            if (message == nullptr || Nintendo3DsRender2DDetailLinesRemaining <= 0) {
                return;
            }

            Nintendo3DsRender2DDetailLinesRemaining--;
            AppendRenderTrace(message);
        }

        /// Releases one deserialized cooked texture asset and its heap-owned pixel arrays after native texture upload completes.
        /// <param name="asset">Cooked texture asset whose serialized data is no longer required.</param>
        void ReleaseTransientTextureAsset(::TextureAsset* asset) {
            if (asset == nullptr) {
                return;
            }

            Array<uint8_t>* colors = asset->Colors;
            Array<uint8_t>* paletteColors = asset->PaletteColors;
            asset->Colors = nullptr;
            asset->PaletteColors = nullptr;
            if (colors != nullptr && colors != Array<uint8_t>::Empty()) {
                delete colors;
            }
            if (paletteColors != nullptr && paletteColors != Array<uint8_t>::Empty()) {
                delete paletteColors;
            }

            delete asset;
        }
    }

    /// Creates the Nintendo 3DS startup 2D renderer for startup-scene queue capture and citro2d playback.
    Nintendo3DsStartupRenderManager2D::Nintendo3DsStartupRenderManager2D()
        : RenderManager2D()
        , ActiveScreenTarget(Nintendo3DsScreenTarget::Top)
        , IsRenderingFrame(false)
        , ActiveViewportOffsetX(0)
        , ActiveViewportOffsetY(0)
        , HasTopScreenClearColor(false)
        , HasBottomScreenClearColor(false)
        , TopScreenClearColor(0)
        , BottomScreenClearColor(0)
        , DrawCommands() {
    }

    /// Releases resources owned by the startup 2D renderer.
    Nintendo3DsStartupRenderManager2D::~Nintendo3DsStartupRenderManager2D() {
    }

    /// Builds one managed runtime texture that mirrors the supplied raw texture dimensions.
    RuntimeTexture* Nintendo3DsStartupRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup texture asset is required.");
        }

        Nintendo3DsRuntimeTexture* runtimeTexture = new Nintendo3DsRuntimeTexture();
        try {
            runtimeTexture->set_Id(data->get_Id());
            runtimeTexture->LoadFromRaw(data);
            return runtimeTexture;
        } catch (...) {
            delete runtimeTexture;
            throw;
        }
    }

    /// Builds one Nintendo 3DS runtime texture for cooked texture requests during startup-scene materialization.
    RuntimeTexture* Nintendo3DsStartupRenderManager2D::BuildTextureFromCooked(HeCppString managedPath, IContentStreamSource* contentStreamSource) {
        if (managedPath.is_null()) throw std::invalid_argument("Nintendo 3DS cooked asset path cannot be null.");
        const std::string cookedAssetPath(managedPath.data(), managedPath.size());
        if (cookedAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked texture asset path is required.");
        }

        ::FileStream* stream = nullptr;
        ::Asset* asset = nullptr;
        ::TextureAsset* cookedTextureAsset = nullptr;
        try {
            if (contentStreamSource != nullptr) {
                ::Stream* contentStream = contentStreamSource->OpenRead(cookedAssetPath);
                stream = dynamic_cast<::FileStream*>(contentStream);
                if (stream == nullptr) {
                    delete contentStream;
                    throw std::runtime_error("Nintendo 3DS cooked texture stream must be one FileStream: " + cookedAssetPath);
                }
            } else {
                stream = ::File::OpenRead(cookedAssetPath);
            }
            asset = ::AssetSerializer::Deserialize(stream);
            delete stream;
            stream = nullptr;

            cookedTextureAsset = he_cpp_try_cast<TextureAsset>(asset);
            if (cookedTextureAsset == nullptr) {
                throw std::invalid_argument("Nintendo 3DS cooked texture payloads must deserialize as TextureAsset.");
            }

            RuntimeTexture* runtimeTexture = BuildTextureFromRaw(cookedTextureAsset);
            ReleaseTransientTextureAsset(cookedTextureAsset);
            cookedTextureAsset = nullptr;
            asset = nullptr;
            return runtimeTexture;
        } catch (...) {
            if (stream != nullptr) {
                delete stream;
            }
            if (cookedTextureAsset != nullptr) {
                ReleaseTransientTextureAsset(cookedTextureAsset);
            } else if (asset != nullptr) {
                delete asset;
            }

            throw;
        }
    }

    /// Captures one sprite draw request in generated-core render order for later rendering.
    void Nintendo3DsStartupRenderManager2D::DrawSprite(ISpriteDrawable2D* sprite) {
        if (sprite == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup sprite drawable is required.");
        }

        DrawCommands.push_back(DrawCommand {
            DrawCommandType::Sprite,
            ActiveScreenTarget,
            ActiveViewportOffsetX,
            ActiveViewportOffsetY,
            sprite,
            Batch2DGeometry::ResolveSpriteQuad(sprite)
        });
    }

    /// Updates validated RGBA8 texels in place without rebuilding the texture or modifying neighbouring atlas glyphs.
    void Nintendo3DsStartupRenderManager2D::UpdateTextureRegionCore(RuntimeTexture* texture, int32_t x, int32_t y,
        int32_t width, int32_t height, Array<uint8_t>* rgba8, int32_t sourceRowPitch) {
        Nintendo3DsRuntimeTexture* native = he_cpp_try_cast<Nintendo3DsRuntimeTexture>(texture);
        if (native == nullptr || native->get_IsDisposed() || !native->HasNativeTexture() || rgba8 == nullptr) {
            throw std::invalid_argument("Nintendo 3DS texture region requires a live native texture and source array.");
        }
        C3D_Tex* gpuTexture = native->GetNativeTexture();
        if (gpuTexture->fmt != GPU_RGBA8 || gpuTexture->data == nullptr) {
            throw std::invalid_argument("Nintendo 3DS texture region updates require RGBA8 GPU storage.");
        }
        ValidateTextureRegion(native->GetActualWidth(), native->GetActualHeight(), x, y, width, height,
            rgba8->Data, rgba8->Length, sourceRowPitch);
        if (IsRenderingFrame) {
            C2D_Flush();
            C3D_FrameSplit(C3D_FRAME_SYNCDRAW);
        } else {
            C3D_FrameSync();
        }
        WriteTextureRegion(static_cast<uint8_t*>(gpuTexture->data), native->GetPaddedWidth(), x, y, width, height,
            rgba8->Data, sourceRowPitch);
        C3D_TexFlush(gpuTexture);
    }

    /// Captures one text draw request in generated-core render order for later rendering.
    void Nintendo3DsStartupRenderManager2D::DrawText(ITextDrawable2D* text) {
        if (text == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text drawable is required.");
        }

        DrawCommands.push_back(DrawCommand {
            DrawCommandType::Text,
            ActiveScreenTarget,
            ActiveViewportOffsetX,
            ActiveViewportOffsetY,
            text
        });
    }

    /// Captures one rounded-rectangle draw request in generated-core render order for later rendering.
    void Nintendo3DsStartupRenderManager2D::DrawRoundedRect(IRoundedRectDrawable2D* shape) {
        if (shape == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup rounded-rectangle drawable is required.");
        }

        DrawCommands.push_back(DrawCommand {
            DrawCommandType::RoundedRect,
            ActiveScreenTarget,
            ActiveViewportOffsetX,
            ActiveViewportOffsetY,
            shape
        });
    }

    /// Walks the active generated-core camera 2D queue and lets each drawable submit itself into the frame capture.
    void Nintendo3DsStartupRenderManager2D::Draw() {
        AppendRenderTrace("Render2D.DrawBoundary: begin");
        Append2DStageMarker("DrawBegin");
        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            Append2DStageMarker("CoreOrObjectManagerMissing");
            AppendRenderTrace("Render2D.DrawBoundary: end");
            return;
        }

        Append2DStageMarker("ObjectManagerResolved");
        List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
        if (cameras == nullptr) {
            Append2DStageMarker("CameraListMissing");
            AppendRenderTrace("Render2D.DrawBoundary: end");
            return;
        }

        Append2DStageMarker("CameraListResolved");
        if (Nintendo3DsRender2DTraceFramesRemaining > 0) {
            char message[256];
            std::snprintf(
                message,
                sizeof(message),
                "Render2D.Draw: cameraCount=%d",
                cameras == nullptr ? -1 : static_cast<int>(cameras->get_Count()));
            AppendRenderTrace(message);
        }
        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            ICamera* camera = (*cameras)[cameraIndex];
            Append2DStageMarker("CameraBegin");
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                Append2DStageMarker("CameraSkipped");
                continue;
            }

            float4 viewport = ResolveCameraViewport(camera);
            int32_t viewportX = 0;
            int32_t viewportY = 0;
            int32_t viewportWidth = 0;
            int32_t viewportHeight = 0;
            Nintendo3DsScreenTarget screenTarget = Nintendo3DsScreenTarget::Top;
            ResolveViewportTarget(viewport, screenTarget, viewportX, viewportY, viewportWidth, viewportHeight);
            if (viewportWidth <= 0 || viewportHeight <= 0) {
                Append2DStageMarker("CameraViewportSkipped");
                continue;
            }

            CameraClearSettings clearSettings = camera->get_ClearSettings();
            if (clearSettings.get_ClearColorEnabled()) {
                u32 clearColor = C2D_Color32f(
                    static_cast<float>(clearSettings.get_ClearColor().X),
                    static_cast<float>(clearSettings.get_ClearColor().Y),
                    static_cast<float>(clearSettings.get_ClearColor().Z),
                    static_cast<float>(clearSettings.get_ClearColor().W));
                if (screenTarget == Nintendo3DsScreenTarget::Top && !HasTopScreenClearColor) {
                    HasTopScreenClearColor = true;
                    TopScreenClearColor = clearColor;
                } else if (screenTarget == Nintendo3DsScreenTarget::Bottom && !HasBottomScreenClearColor) {
                    HasBottomScreenClearColor = true;
                    BottomScreenClearColor = clearColor;
                }
            }

            IRenderQueue2D* renderQueue = camera->get_RenderQueue2D();
            if (renderQueue == nullptr) {
                AppendRenderTrace("Render2D.DrawCamera: renderQueue=null");
                Append2DStageMarker("RenderQueueMissing");
                continue;
            }

            ActiveScreenTarget = screenTarget;
            ActiveViewportOffsetX = viewportX;
            ActiveViewportOffsetY = viewportY;
            if (Nintendo3DsRender2DTraceFramesRemaining > 0) {
                char message[256];
                std::snprintf(
                    message,
                    sizeof(message),
                    "Render2D.DrawCamera: screen=%s queueCount=%d viewport=(%d,%d,%d,%d)",
                    screenTarget == Nintendo3DsScreenTarget::Top ? "top" : "bottom",
                    static_cast<int>(renderQueue->get_Count()),
                    static_cast<int>(viewportX),
                    static_cast<int>(viewportY),
                    static_cast<int>(viewportWidth),
                    static_cast<int>(viewportHeight));
                AppendRenderTrace(message);
            }
            Append2DStageMarker("QueueVisitBegin");
            renderQueue->VisitOrdered(this);
            Append2DStageMarker("QueueVisitComplete");
        }

        Append2DStageMarker("DrawComplete");
        AppendRenderTrace("Render2D.DrawBoundary: end");
    }

    /// Visits one ordered 2D drawable from the active generated-core camera queue.
    void Nintendo3DsStartupRenderManager2D::Visit(IDrawable2D* drawable) {
        if (drawable == nullptr || drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        drawable->Draw();
    }

    /// Clears the previous frame's captured 2D draw requests before the next generated-core update begins.
    void Nintendo3DsStartupRenderManager2D::BeginFrame() {
        DrawCommands.clear();
        ActiveScreenTarget = Nintendo3DsScreenTarget::Top;
        ActiveViewportOffsetX = 0;
        ActiveViewportOffsetY = 0;
        HasTopScreenClearColor = false;
        HasBottomScreenClearColor = false;
        TopScreenClearColor = 0;
        BottomScreenClearColor = 0;
    }

    /// Draws the captured startup-scene 2D content onto the active top-screen citro2d scene.
    void Nintendo3DsStartupRenderManager2D::RenderTopScreen() {
        IsRenderingFrame = true;
        for (const DrawCommand& command : DrawCommands) {
            if (command.Drawable == nullptr || command.ScreenTarget != Nintendo3DsScreenTarget::Top) {
                continue;
            }

            ActiveScreenTarget = command.ScreenTarget;
            ActiveViewportOffsetX = command.ViewportOffsetX;
            ActiveViewportOffsetY = command.ViewportOffsetY;
            switch (command.Type) {
                case DrawCommandType::Sprite:
                    RenderSprite(static_cast<ISpriteDrawable2D*>(command.Drawable), command.Geometry);
                    break;
                case DrawCommandType::Text:
                    RenderText(static_cast<ITextDrawable2D*>(command.Drawable));
                    break;
                case DrawCommandType::RoundedRect:
                    RenderRoundedRect(static_cast<IRoundedRectDrawable2D*>(command.Drawable));
                    break;
            }
        }
        IsRenderingFrame = false;
    }

    /// Draws the captured startup-scene 2D content onto the active bottom-screen citro2d scene.
    void Nintendo3DsStartupRenderManager2D::RenderBottomScreen() {
        IsRenderingFrame = true;
        for (const DrawCommand& command : DrawCommands) {
            if (command.Drawable == nullptr || command.ScreenTarget != Nintendo3DsScreenTarget::Bottom) {
                continue;
            }

            ActiveScreenTarget = command.ScreenTarget;
            ActiveViewportOffsetX = command.ViewportOffsetX;
            ActiveViewportOffsetY = command.ViewportOffsetY;
            switch (command.Type) {
                case DrawCommandType::Sprite:
                    RenderSprite(static_cast<ISpriteDrawable2D*>(command.Drawable), command.Geometry);
                    break;
                case DrawCommandType::Text:
                    RenderText(static_cast<ITextDrawable2D*>(command.Drawable));
                    break;
                case DrawCommandType::RoundedRect:
                    RenderRoundedRect(static_cast<IRoundedRectDrawable2D*>(command.Drawable));
                    break;
            }
        }

        IsRenderingFrame = false;
        if (Nintendo3DsRender2DTraceFramesRemaining > 0) {
            char message[256];
            std::snprintf(
                message,
                sizeof(message),
                "Render2D.Render: commands=%u hasTopClear=%s hasBottomClear=%s",
                static_cast<unsigned>(DrawCommands.size()),
                HasTopScreenClearColor ? "true" : "false",
                HasBottomScreenClearColor ? "true" : "false");
            AppendRenderTrace(message);
            Nintendo3DsRender2DTraceFramesRemaining--;
        }
    }

    /// Resolves the active top-screen clear color from the first enabled generated-core camera that requests one.
    u32 Nintendo3DsStartupRenderManager2D::ResolveTopScreenClearColor(u32 fallbackColor) const {
        return HasTopScreenClearColor ? TopScreenClearColor : fallbackColor;
    }

    /// Resolves the active bottom-screen clear color from the first enabled generated-core camera that requests one.
    u32 Nintendo3DsStartupRenderManager2D::ResolveBottomScreenClearColor(u32 fallbackColor) const {
        return HasBottomScreenClearColor ? BottomScreenClearColor : fallbackColor;
    }

    /// Converts one generated-core byte color into the citro2d RGBA layout expected by the Nintendo 3DS GPU.
    u32 Nintendo3DsStartupRenderManager2D::ConvertColor(byte4 color) const {
        return C2D_Color32(color.X, color.Y, color.Z, color.W);
    }

    /// Resolves one text string using generated-core wrapping rules when authored wrapping is enabled.
    std::string Nintendo3DsStartupRenderManager2D::ResolveTextContent(ITextDrawable2D* text) const {
        if (text == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text content requires one drawable.");
        }

        const auto managedContent = text->get_Text();
        if (managedContent.is_null()) throw std::invalid_argument("Nintendo 3DS text content cannot be null.");
        std::string content(managedContent.data(), managedContent.size());
        if (!text->get_WrapText()) {
            return content;
        }

        FontAsset* font = text->get_Font();
        if (font == nullptr || text->get_Size().X <= 0) {
            return content;
        }

        const RealEvaluation scale = ResolveTextScale(text);
        const int32_t wrapWidth = std::max<int32_t>(1, static_cast<int32_t>(RealMath::Round(RealEvaluation(text->get_Size().X) / scale, MidpointRounding::AwayFromZero)));
        const auto wrapped = TextLayoutUtils::WrapText(content, font, wrapWidth);
        return std::string(wrapped.data(), wrapped.size());
    }

    /// Resolves one generated-core font scale for Nintendo 3DS text glyph placement.
    RealEvaluation Nintendo3DsStartupRenderManager2D::ResolveTextScale(ITextDrawable2D* text) const {
        if (text == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text scale requires one drawable.");
        }

        RealEvaluation scale = text->get_FontScale();
        if (scale <= 0.0f) {
            scale = 1.0f;
        }

        return RealMath::Max(0.0001f, scale);
    }

    /// Builds one horizontal offset per rendered text line so authored alignment is respected across wrapped and non-wrapped content.
    std::vector<RealEvaluation> Nintendo3DsStartupRenderManager2D::BuildTextLineOffsets(
        ITextDrawable2D* text,
        FontAsset* font,
        const std::string& content,
        RealEvaluation scale,
        RealEvaluation textureWidth) const {
        if (text == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text alignment requires one drawable.");
        } else if (font == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text alignment requires one font.");
        } else if (scale <= 0.0f) {
            throw std::invalid_argument("Nintendo 3DS startup text alignment requires one positive glyph scale.");
        } else if (textureWidth <= 0.0f) {
            throw std::invalid_argument("Nintendo 3DS startup text alignment requires one positive texture width.");
        }

        std::vector<RealEvaluation> lineOffsets;
        std::string currentLine;
        currentLine.reserve(content.size());
        for (char character : content) {
            if (character == '\n') {
                RealEvaluation visibleWidth = MeasureVisibleTextLineWidth(currentLine, font, scale, textureWidth);
                lineOffsets.push_back(ResolveAlignedTextOffset(static_cast<int32_t>(text->get_Alignment()), text->get_Size().X, visibleWidth));
                currentLine.clear();
                continue;
            }

            currentLine.push_back(character);
        }

        RealEvaluation visibleWidth = MeasureVisibleTextLineWidth(currentLine, font, scale, textureWidth);
        lineOffsets.push_back(ResolveAlignedTextOffset(static_cast<int32_t>(text->get_Alignment()), text->get_Size().X, visibleWidth));
        return lineOffsets;
    }

    /// Measures the visible glyph width of one rendered text line.
    RealEvaluation Nintendo3DsStartupRenderManager2D::MeasureVisibleTextLineWidth(
        const std::string& line,
        FontAsset* font,
        RealEvaluation scale,
        RealEvaluation textureWidth) const {
        if (font == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text width measurement requires one font.");
        } else if (scale <= 0.0f) {
            throw std::invalid_argument("Nintendo 3DS startup text width measurement requires one positive glyph scale.");
        } else if (textureWidth <= 0.0f) {
            throw std::invalid_argument("Nintendo 3DS startup text width measurement requires one positive texture width.");
        }

        RealEvaluation visibleWidth = 0.0f;
        RealEvaluation offsetX = 0.0f;
        const RealEvaluation spaceWidth = font->get_FontInfo() != nullptr
            ? RealMath::Max(font->get_FontInfo()->get_SpaceWidth() * scale, 1.0f)
            : RealMath::Max(font->get_LineHeight() * scale * 0.25f, 1.0f);
        for (char character : line) {
            if (character == ' ') {
                offsetX += spaceWidth;
                visibleWidth = RealMath::Max(visibleWidth, offsetX);
                continue;
            }

            FontChar glyph;
            if (font->get_Characters() == nullptr || !font->get_Characters()->TryGetValue(character, glyph)) {
                continue;
            }

            const RealEvaluation glyphWidth = RealMath::Max(1.0f, glyph.SourceRect.Z * textureWidth * scale);
            visibleWidth = RealMath::Max(visibleWidth, offsetX + glyphWidth);
            offsetX += glyph.AdvanceWidth > 0.0f
                ? glyph.AdvanceWidth * scale
                : glyphWidth;
        }

        return visibleWidth;
    }

    /// Resolves the horizontal offset required to place one line according to its authored text alignment.
    RealEvaluation Nintendo3DsStartupRenderManager2D::ResolveAlignedTextOffset(int32_t alignmentValue, int32_t boxWidth, RealEvaluation visibleWidth) const {
        if (boxWidth <= 0 || visibleWidth <= 0.0f) {
            return 0.0f;
        }

        const RealEvaluation availableWidth = RealEvaluation(boxWidth) - visibleWidth;
        if (availableWidth <= 0.0f) {
            return 0.0f;
        }

        switch (static_cast<TextAlignment>(alignmentValue)) {
            case TextAlignment::Center:
                return availableWidth * 0.5f;

            case TextAlignment::Right:
                return availableWidth;

            case TextAlignment::Left:
            default:
                return 0.0f;
        }
    }

    /// Resolves one authored camera viewport into Nintendo 3DS pixel-space bounds.
    float4 Nintendo3DsStartupRenderManager2D::ResolveCameraViewport(ICamera* camera) const {
        if (camera == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup camera is required.");
        }

        float4 viewport = camera->get_Viewport();
        if (viewport.Z <= 1.0f && viewport.W <= 1.0f) {
            const bool targetsBottomScreen = viewport.Y >= 1.0f;
            const float targetScreenWidth = targetsBottomScreen
                ? static_cast<float>(Nintendo3DsBottomScreenWidth)
                : static_cast<float>(Nintendo3DsTopScreenWidth);
            return float4(
                viewport.X * targetScreenWidth,
                viewport.Y * Nintendo3DsTopScreenHeight,
                viewport.Z * targetScreenWidth,
                viewport.W * Nintendo3DsTopScreenHeight);
        }

        return viewport;
    }

    /// Resolves the target screen and screen-local viewport offsets for one Nintendo 3DS camera viewport.
    void Nintendo3DsStartupRenderManager2D::ResolveViewportTarget(
        const float4& viewport,
        Nintendo3DsScreenTarget& screenTarget,
        int32_t& viewportX,
        int32_t& viewportY,
        int32_t& viewportWidth,
        int32_t& viewportHeight) const {
        viewportX = static_cast<int32_t>(::RealMath::Round(viewport.X, ::MidpointRounding::AwayFromZero));
        int32_t resolvedViewportY = static_cast<int32_t>(::RealMath::Round(viewport.Y, ::MidpointRounding::AwayFromZero));
        viewportWidth = std::max(static_cast<int32_t>(0), static_cast<int32_t>(::RealMath::Round(viewport.Z, ::MidpointRounding::AwayFromZero)));
        viewportHeight = std::max(static_cast<int32_t>(0), static_cast<int32_t>(::RealMath::Round(viewport.W, ::MidpointRounding::AwayFromZero)));
        screenTarget = resolvedViewportY >= Nintendo3DsTopScreenHeight
            ? Nintendo3DsScreenTarget::Bottom
            : Nintendo3DsScreenTarget::Top;
        viewportY = screenTarget == Nintendo3DsScreenTarget::Bottom
            ? resolvedViewportY - Nintendo3DsTopScreenHeight
            : resolvedViewportY;

        if (screenTarget == Nintendo3DsScreenTarget::Top && resolvedViewportY + viewportHeight > Nintendo3DsTopScreenHeight) {
            throw std::invalid_argument("Nintendo 3DS 2D cameras must not span from the top screen into the bottom screen.");
        } else if (screenTarget == Nintendo3DsScreenTarget::Bottom && viewportY + viewportHeight > Nintendo3DsBottomScreenHeight) {
            throw std::invalid_argument("Nintendo 3DS 2D cameras must stay fully inside the bottom screen.");
        }
    }

    /// Renders one captured sprite draw request onto the active top-screen citro2d scene.
    void Nintendo3DsStartupRenderManager2D::RenderSprite(ISpriteDrawable2D* sprite, const SpriteQuad2D& quad) {
        if (sprite == nullptr || sprite->get_Parent() == nullptr) {
            AppendRenderTrace("Render2D.RenderSprite: sprite-or-parent-null");
            return;
        }

        Nintendo3DsRuntimeTexture* runtimeTexture = dynamic_cast<Nintendo3DsRuntimeTexture*>(sprite->get_Texture());
        if (runtimeTexture == nullptr || !runtimeTexture->HasNativeTexture()) {
            AppendRenderTrace("Render2D.RenderSprite: missing-native-texture");
            return;
        }

        std::string textureTraceSummary;
        if (runtimeTexture->TryTakeDebugTraceSummary(textureTraceSummary)) {
            std::string textureTraceMessage = "Render2D.TextureSample.Sprite: " + textureTraceSummary;
            AppendDetailedRenderTrace(textureTraceMessage.c_str());
        }

        float4 sourceRect = sprite->get_SourceRect();
        float normalizedWidth = static_cast<float>(runtimeTexture->GetActualWidth()) / static_cast<float>(runtimeTexture->GetPaddedWidth());
        float normalizedHeight = static_cast<float>(runtimeTexture->GetActualHeight()) / static_cast<float>(runtimeTexture->GetPaddedHeight());
        Tex3DS_SubTexture subTexture;
        subTexture.width = static_cast<uint16_t>(std::max<int32_t>(1, static_cast<int32_t>(::RealMath::Round(sourceRect.Z * runtimeTexture->GetActualWidth(), ::MidpointRounding::AwayFromZero))));
        subTexture.height = static_cast<uint16_t>(std::max<int32_t>(1, static_cast<int32_t>(::RealMath::Round(sourceRect.W * runtimeTexture->GetActualHeight(), ::MidpointRounding::AwayFromZero))));
        subTexture.left = static_cast<float>(sourceRect.X * normalizedWidth);
        subTexture.right = static_cast<float>(subTexture.left + (sourceRect.Z * normalizedWidth));
        subTexture.top = static_cast<float>(1.0f - (sourceRect.Y * normalizedHeight));
        subTexture.bottom = static_cast<float>(subTexture.top - (sourceRect.W * normalizedHeight));

        C2D_Image image;
        image.tex = runtimeTexture->GetNativeTexture();
        image.subtex = &subTexture;

        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, ConvertColor(sprite->get_Color()), 1.0f);
        C2D_SetTintMode(C2D_TintMult);

        C3D_Mtx previousView;
        C3D_Mtx spriteView;
        C3D_Mtx combinedView;
        C2D_ViewSave(&previousView);
        Nintendo3DsSpriteGeometry::WriteUnitQuadMatrix(spriteView,
            static_cast<float>(quad.TopLeft.X), static_cast<float>(quad.TopLeft.Y), static_cast<float>(quad.TopRight.X), static_cast<float>(quad.TopRight.Y),
            static_cast<float>(quad.BottomLeft.X), static_cast<float>(quad.BottomLeft.Y),
            static_cast<float>(ActiveViewportOffsetX), static_cast<float>(ActiveViewportOffsetY));
        Mtx_Multiply(&combinedView, &previousView, &spriteView);
        C2D_ViewRestore(&combinedView);
        C2D_DrawParams drawParams {};
        drawParams.pos.w = 1.0f;
        drawParams.pos.h = 1.0f;
        const bool drawn = C2D_DrawImage(image, &drawParams, &tint);
        C2D_ViewRestore(&previousView);
        if (!drawn) { throw std::runtime_error("Nintendo 3DS sprite image submission failed."); }
    }

    /// Renders one captured text draw request onto the active top-screen citro2d scene.
    void Nintendo3DsStartupRenderManager2D::RenderText(ITextDrawable2D* text) {
        if (text == nullptr || text->get_Parent() == nullptr) {
            AppendRenderTrace("Render2D.RenderText: text-or-parent-null");
            return;
        }

        std::string content = ResolveTextContent(text);
        if (content.empty()) {
            AppendRenderTrace("Render2D.RenderText: empty-content");
            return;
        }

        FontAsset* font = text->get_Font();
        if (font == nullptr) {
            AppendRenderTrace("Render2D.RenderText: missing-font");
            return;
        }

        Nintendo3DsRuntimeTexture* runtimeTexture = dynamic_cast<Nintendo3DsRuntimeTexture*>(font->get_Texture());
        if (runtimeTexture == nullptr || !runtimeTexture->HasNativeTexture()) {
            AppendRenderTrace("Render2D.RenderText: missing-native-font-texture");
            return;
        }

        std::string textureTraceSummary;
        if (runtimeTexture->TryTakeDebugTraceSummary(textureTraceSummary)) {
            std::string textureTraceMessage = "Render2D.TextureSample.Font: " + textureTraceSummary;
            AppendDetailedRenderTrace(textureTraceMessage.c_str());
        }

        float3 position = text->get_Parent()->get_Position();
        const RealEvaluation scale = ResolveTextScale(text);
        const RealEvaluation textureWidth = runtimeTexture->GetActualWidth();
        const RealEvaluation textureHeight = runtimeTexture->GetActualHeight();
        if (textureWidth <= 0.0f || textureHeight <= 0.0f) {
            AppendRenderTrace("Render2D.RenderText: invalid-font-texture-size");
            return;
        }

        const RealEvaluation lineHeight = ::RealMath::Max(font->get_LineHeight() * scale, 1.0f);
        const RealEvaluation spaceWidth = font->get_FontInfo() != nullptr
            ? ::RealMath::Max(font->get_FontInfo()->get_SpaceWidth() * scale, 1.0f)
            : ::RealMath::Max(lineHeight * 0.25f, 1.0f);
        std::vector<RealEvaluation> lineOffsets = BuildTextLineOffsets(text, font, content, scale, textureWidth);
        RealEvaluation renderOriginX = position.X + ActiveViewportOffsetX;
        RealEvaluation offsetX = 0.0f;
        RealEvaluation offsetY = 0.0f;
        size_t lineIndex = 0;
        RealEvaluation lineOriginX = renderOriginX + (!lineOffsets.empty() ? lineOffsets[0] : RealEvaluation(0.0f));
        char textTraceMessage[256];
        std::snprintf(
            textTraceMessage,
            sizeof(textTraceMessage),
            "Render2D.RenderText: pos=(%.1f,%.1f) scale=%.2f contentLength=%u traceBytes=%u",
            static_cast<double>(renderOriginX),
            static_cast<double>(position.Y + ActiveViewportOffsetY),
            static_cast<double>(scale),
            static_cast<unsigned>(content.size()),
            static_cast<unsigned>(runtimeTexture->GetDebugTraceSummary().size()));
        AppendDetailedRenderTrace(textTraceMessage);
        C2D_SetTintMode(C2D_TintMult);
        bool hasFirstGlyphTrace = false;
        float4 firstGlyphSourceRect;
        float4 firstGlyphDrawBounds;
        float4 firstGlyphSubTextureBounds;
        char firstGlyphCharacter = '\0';
        int32_t missingGlyphCount = 0;
        int32_t drawnGlyphCount = 0;
        for (char character : content) {
            if (character == '\n') {
                offsetY += lineHeight;
                offsetX = 0.0f;
                lineIndex++;
                lineOriginX = renderOriginX + (lineIndex < lineOffsets.size() ? lineOffsets[lineIndex] : RealEvaluation(0.0f));
                continue;
            }

            if (character == ' ') {
                offsetX += spaceWidth;
                continue;
            }

            FontChar glyph;
            if (font->get_Characters() == nullptr || !font->get_Characters()->TryGetValue(character, glyph)) {
                missingGlyphCount++;
                continue;
            }

            float4 sourceRect = glyph.SourceRect;
            Tex3DS_SubTexture subTexture;
            subTexture.width = static_cast<uint16_t>(static_cast<int32_t>(::RealMath::Max(1.0f, ::RealMath::Round(sourceRect.Z * textureWidth, ::MidpointRounding::AwayFromZero))));
            subTexture.height = static_cast<uint16_t>(static_cast<int32_t>(::RealMath::Max(1.0f, ::RealMath::Round(sourceRect.W * textureHeight, ::MidpointRounding::AwayFromZero))));
            subTexture.left = static_cast<float>(sourceRect.X * (textureWidth / static_cast<float>(runtimeTexture->GetPaddedWidth())));
            subTexture.right = static_cast<float>(subTexture.left + (sourceRect.Z * (textureWidth / static_cast<float>(runtimeTexture->GetPaddedWidth()))));
            subTexture.top = static_cast<float>(1.0f - (sourceRect.Y * (textureHeight / static_cast<float>(runtimeTexture->GetPaddedHeight()))));
            subTexture.bottom = static_cast<float>(subTexture.top - (sourceRect.W * (textureHeight / static_cast<float>(runtimeTexture->GetPaddedHeight()))));

            C2D_Image image;
            image.tex = runtimeTexture->GetNativeTexture();
            image.subtex = &subTexture;

            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, ConvertColor(text->get_Color()), 1.0f);

            C2D_DrawParams drawParams;
            drawParams.pos.x = static_cast<float>(lineOriginX + offsetX);
            drawParams.pos.y = static_cast<float>(position.Y + ActiveViewportOffsetY + offsetY + (glyph.OffsetY * scale));
            drawParams.pos.w = static_cast<float>(::RealMath::Max(1.0f, sourceRect.Z * textureWidth * scale));
            drawParams.pos.h = static_cast<float>(::RealMath::Max(1.0f, sourceRect.W * textureHeight * scale));
            drawParams.center.x = 0.0f;
            drawParams.center.y = 0.0f;
            drawParams.depth = 0.0f;
            drawParams.angle = 0.0f;
            if (!hasFirstGlyphTrace) {
                hasFirstGlyphTrace = true;
                firstGlyphCharacter = character;
                firstGlyphSourceRect = sourceRect;
                firstGlyphDrawBounds = float4(drawParams.pos.x, drawParams.pos.y, drawParams.pos.w, drawParams.pos.h);
                firstGlyphSubTextureBounds = float4(subTexture.left, subTexture.top, subTexture.right, subTexture.bottom);
            }
            bool glyphDrawn = C2D_DrawImage(image, &drawParams, &tint);
            drawnGlyphCount++;
            if (!glyphDrawn) {
                AppendDetailedRenderTrace("Render2D.RenderTextGlyphs: draw-image=false");
            }

            offsetX += glyph.AdvanceWidth > 0.0f
                ? glyph.AdvanceWidth * scale
                : RealEvaluation(drawParams.pos.w);
        }

        if (hasFirstGlyphTrace) {
            char glyphTraceMessage[512];
            std::snprintf(
                glyphTraceMessage,
                sizeof(glyphTraceMessage),
                "Render2D.RenderTextGlyphs: first='%c' src=(%.4f,%.4f,%.4f,%.4f) dst=(%.1f,%.1f,%.1f,%.1f) sub=(%.4f,%.4f,%.4f,%.4f) drawn=%d missing=%d",
                firstGlyphCharacter,
                static_cast<double>(firstGlyphSourceRect.X),
                static_cast<double>(firstGlyphSourceRect.Y),
                static_cast<double>(firstGlyphSourceRect.Z),
                static_cast<double>(firstGlyphSourceRect.W),
                static_cast<double>(firstGlyphDrawBounds.X),
                static_cast<double>(firstGlyphDrawBounds.Y),
                static_cast<double>(firstGlyphDrawBounds.Z),
                static_cast<double>(firstGlyphDrawBounds.W),
                static_cast<double>(firstGlyphSubTextureBounds.X),
                static_cast<double>(firstGlyphSubTextureBounds.Y),
                static_cast<double>(firstGlyphSubTextureBounds.Z),
                static_cast<double>(firstGlyphSubTextureBounds.W),
                static_cast<int>(drawnGlyphCount),
                static_cast<int>(missingGlyphCount));
            AppendDetailedRenderTrace(glyphTraceMessage);
        } else {
            char glyphTraceMessage[256];
            std::snprintf(
                glyphTraceMessage,
                sizeof(glyphTraceMessage),
                "Render2D.RenderTextGlyphs: no-drawable-glyphs missing=%d",
                static_cast<int>(missingGlyphCount));
            AppendDetailedRenderTrace(glyphTraceMessage);
        }
    }

    /// Renders one captured rounded-rectangle draw request onto the active top-screen citro2d scene.
    void Nintendo3DsStartupRenderManager2D::RenderRoundedRect(IRoundedRectDrawable2D* shape) {
        if (shape == nullptr || shape->get_Parent() == nullptr) {
            return;
        }

        float3 position = shape->get_Parent()->get_Position();
        int2 size = shape->get_Size();
        if (size.X <= 0 || size.Y <= 0) {
            return;
        }

        u32 resolvedFillColor = ConvertColor(shape->get_FillColor());
        C2D_DrawRectSolid(
            static_cast<float>(position.X + ActiveViewportOffsetX),
            static_cast<float>(position.Y + ActiveViewportOffsetY),
            0.0f,
            static_cast<float>(size.X),
            static_cast<float>(size.Y),
            resolvedFillColor);

        if (shape->get_BorderThickness() <= 0.0f || shape->get_BorderColor().W == 0) {
            return;
        }

        const float borderThickness = static_cast<float>(::RealMath::Max(1.0f, ::RealMath::Round(shape->get_BorderThickness(), ::MidpointRounding::AwayFromZero)));
        const float width = static_cast<float>(size.X);
        const float height = static_cast<float>(size.Y);
        const u32 borderColor = ConvertColor(shape->get_BorderColor());
        float viewportOriginX = static_cast<float>(position.X + ActiveViewportOffsetX);
        float viewportOriginY = static_cast<float>(position.Y + ActiveViewportOffsetY);
        char roundedRectTraceMessage[256];
        std::snprintf(
            roundedRectTraceMessage,
            sizeof(roundedRectTraceMessage),
            "Render2D.RenderRoundedRect: pos=(%.1f,%.1f) size=(%.1f,%.1f)",
            viewportOriginX,
            viewportOriginY,
            width,
            height);
        AppendDetailedRenderTrace(roundedRectTraceMessage);
        C2D_DrawRectSolid(viewportOriginX, viewportOriginY, 0.0f, width, borderThickness, borderColor);
        C2D_DrawRectSolid(viewportOriginX, viewportOriginY + height - borderThickness, 0.0f, width, borderThickness, borderColor);
        C2D_DrawRectSolid(viewportOriginX, viewportOriginY, 0.0f, borderThickness, height, borderColor);
        C2D_DrawRectSolid(viewportOriginX + width - borderThickness, viewportOriginY, 0.0f, borderThickness, height, borderColor);
    }
}

#endif
