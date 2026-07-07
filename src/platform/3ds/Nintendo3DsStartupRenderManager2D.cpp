#include "platform/3ds/Nintendo3DsStartupRenderManager2D.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <cstdio>
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "FontAsset.hpp"
#include "FontChar.hpp"
#include "FontInfo.hpp"
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
#include "runtime/native_cast.hpp"
#include "system/io/file.hpp"

namespace helengine::nintendo3ds {
    namespace {
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

        /// Appends one diagnostic line to the shared Nintendo 3DS renderer trace file.
        /// <param name="message">Trace line that describes one 2D renderer boundary.</param>
        void AppendRenderTrace(const char* message) {
            if (message == nullptr || Nintendo3DsRender2DTraceFramesRemaining <= 0) {
                return;
            }

            std::FILE* file = std::fopen(Nintendo3DsRenderTracePath, "a");
            if (file == nullptr) {
                return;
            }

            std::fputs(message, file);
            std::fputc('\n', file);
            std::fclose(file);
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
    }

    /// Creates the Nintendo 3DS startup 2D renderer for startup-scene queue capture and citro2d playback.
    Nintendo3DsStartupRenderManager2D::Nintendo3DsStartupRenderManager2D()
        : RenderManager2D()
        , ActiveScreenTarget(Nintendo3DsScreenTarget::Top)
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
    RuntimeTexture* Nintendo3DsStartupRenderManager2D::BuildTextureFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) {
        (void)contentStreamSource;
        if (cookedAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked texture asset path is required.");
        }

        ::FileStream* stream = nullptr;
        ::Asset* asset = nullptr;
        try {
            stream = ::File::OpenRead(cookedAssetPath);
            asset = ::AssetSerializer::Deserialize(stream);
            delete stream;
            stream = nullptr;

            ::TextureAsset* cookedTextureAsset = he_cpp_try_cast<TextureAsset>(asset);
            if (cookedTextureAsset == nullptr) {
                throw std::invalid_argument("Nintendo 3DS cooked texture payloads must deserialize as TextureAsset.");
            }

            RuntimeTexture* runtimeTexture = BuildTextureFromRaw(cookedTextureAsset);
            delete cookedTextureAsset;
            return runtimeTexture;
        } catch (...) {
            if (stream != nullptr) {
                delete stream;
            }
            if (asset != nullptr) {
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
            sprite
        });
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
        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            return;
        }

        List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
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
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
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
                continue;
            }

            CameraClearSettings clearSettings = camera->get_ClearSettings();
            if (clearSettings.get_ClearColorEnabled()) {
                u32 clearColor = C2D_Color32f(
                    clearSettings.get_ClearColor().X,
                    clearSettings.get_ClearColor().Y,
                    clearSettings.get_ClearColor().Z,
                    clearSettings.get_ClearColor().W);
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
            renderQueue->VisitOrdered(this);
        }
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
        for (const DrawCommand& command : DrawCommands) {
            if (command.Drawable == nullptr || command.ScreenTarget != Nintendo3DsScreenTarget::Top) {
                continue;
            }

            ActiveScreenTarget = command.ScreenTarget;
            ActiveViewportOffsetX = command.ViewportOffsetX;
            ActiveViewportOffsetY = command.ViewportOffsetY;
            switch (command.Type) {
                case DrawCommandType::Sprite:
                    RenderSprite(static_cast<ISpriteDrawable2D*>(command.Drawable));
                    break;
                case DrawCommandType::Text:
                    RenderText(static_cast<ITextDrawable2D*>(command.Drawable));
                    break;
                case DrawCommandType::RoundedRect:
                    RenderRoundedRect(static_cast<IRoundedRectDrawable2D*>(command.Drawable));
                    break;
            }
        }
    }

    /// Draws the captured startup-scene 2D content onto the active bottom-screen citro2d scene.
    void Nintendo3DsStartupRenderManager2D::RenderBottomScreen() {
        for (const DrawCommand& command : DrawCommands) {
            if (command.Drawable == nullptr || command.ScreenTarget != Nintendo3DsScreenTarget::Bottom) {
                continue;
            }

            ActiveScreenTarget = command.ScreenTarget;
            ActiveViewportOffsetX = command.ViewportOffsetX;
            ActiveViewportOffsetY = command.ViewportOffsetY;
            switch (command.Type) {
                case DrawCommandType::Sprite:
                    RenderSprite(static_cast<ISpriteDrawable2D*>(command.Drawable));
                    break;
                case DrawCommandType::Text:
                    RenderText(static_cast<ITextDrawable2D*>(command.Drawable));
                    break;
                case DrawCommandType::RoundedRect:
                    RenderRoundedRect(static_cast<IRoundedRectDrawable2D*>(command.Drawable));
                    break;
            }
        }

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

    /// Resolves one sprite draw size from authored size overrides or runtime texture dimensions.
    float4 Nintendo3DsStartupRenderManager2D::ResolveSpriteBounds(ISpriteDrawable2D* sprite) const {
        if (sprite == nullptr || sprite->get_Parent() == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup sprite bounds require one drawable with one parent.");
        }

        int2 size = sprite->get_Size();
        RuntimeTexture* texture = sprite->get_Texture();
        float width = size.X > 0 ? static_cast<float>(size.X) : (texture != nullptr ? static_cast<float>(texture->get_Width()) : 0.0f);
        float height = size.Y > 0 ? static_cast<float>(size.Y) : (texture != nullptr ? static_cast<float>(texture->get_Height()) : 0.0f);
        float3 position = sprite->get_Parent()->get_Position();
        return float4(position.X, position.Y, width, height);
    }

    /// Resolves one text string using generated-core wrapping rules when authored wrapping is enabled.
    std::string Nintendo3DsStartupRenderManager2D::ResolveTextContent(ITextDrawable2D* text) const {
        if (text == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text content requires one drawable.");
        }

        std::string content = text->get_Text();
        if (!text->get_WrapText()) {
            return content;
        }

        FontAsset* font = text->get_Font();
        if (font == nullptr || text->get_Size().X <= 0) {
            return content;
        }

        const float scale = ResolveTextScale(text);
        const int32_t wrapWidth = std::max<int32_t>(1, static_cast<int32_t>(std::lround(static_cast<double>(text->get_Size().X) / scale)));
        return TextLayoutUtils::WrapText(content, font, wrapWidth);
    }

    /// Resolves one generated-core font scale for Nintendo 3DS text glyph placement.
    float Nintendo3DsStartupRenderManager2D::ResolveTextScale(ITextDrawable2D* text) const {
        if (text == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text scale requires one drawable.");
        }

        float scale = text->get_FontScale();
        if (scale <= 0.0f) {
            scale = 1.0f;
        }

        return std::max(0.0001f, scale);
    }

    /// Builds one horizontal offset per rendered text line so authored alignment is respected across wrapped and non-wrapped content.
    std::vector<float> Nintendo3DsStartupRenderManager2D::BuildTextLineOffsets(
        ITextDrawable2D* text,
        FontAsset* font,
        const std::string& content,
        float scale,
        float textureWidth) const {
        if (text == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text alignment requires one drawable.");
        } else if (font == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text alignment requires one font.");
        } else if (scale <= 0.0f) {
            throw std::invalid_argument("Nintendo 3DS startup text alignment requires one positive glyph scale.");
        } else if (textureWidth <= 0.0f) {
            throw std::invalid_argument("Nintendo 3DS startup text alignment requires one positive texture width.");
        }

        std::vector<float> lineOffsets;
        std::string currentLine;
        currentLine.reserve(content.size());
        for (char character : content) {
            if (character == '\n') {
                float visibleWidth = MeasureVisibleTextLineWidth(currentLine, font, scale, textureWidth);
                lineOffsets.push_back(ResolveAlignedTextOffset(static_cast<int32_t>(text->get_Alignment()), text->get_Size().X, visibleWidth));
                currentLine.clear();
                continue;
            }

            currentLine.push_back(character);
        }

        float visibleWidth = MeasureVisibleTextLineWidth(currentLine, font, scale, textureWidth);
        lineOffsets.push_back(ResolveAlignedTextOffset(static_cast<int32_t>(text->get_Alignment()), text->get_Size().X, visibleWidth));
        return lineOffsets;
    }

    /// Measures the visible glyph width of one rendered text line.
    float Nintendo3DsStartupRenderManager2D::MeasureVisibleTextLineWidth(
        const std::string& line,
        FontAsset* font,
        float scale,
        float textureWidth) const {
        if (font == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup text width measurement requires one font.");
        } else if (scale <= 0.0f) {
            throw std::invalid_argument("Nintendo 3DS startup text width measurement requires one positive glyph scale.");
        } else if (textureWidth <= 0.0f) {
            throw std::invalid_argument("Nintendo 3DS startup text width measurement requires one positive texture width.");
        }

        float visibleWidth = 0.0f;
        float offsetX = 0.0f;
        const float spaceWidth = font->get_FontInfo() != nullptr
            ? std::max(font->get_FontInfo()->get_SpaceWidth() * scale, 1.0f)
            : std::max(font->get_LineHeight() * scale * 0.25f, 1.0f);
        for (char character : line) {
            if (character == ' ') {
                offsetX += spaceWidth;
                visibleWidth = std::max(visibleWidth, offsetX);
                continue;
            }

            FontChar glyph;
            if (font->get_Characters() == nullptr || !font->get_Characters()->TryGetValue(character, glyph)) {
                continue;
            }

            const float glyphWidth = std::max(1.0f, glyph.SourceRect.Z * textureWidth * scale);
            visibleWidth = std::max(visibleWidth, offsetX + glyphWidth);
            offsetX += glyph.AdvanceWidth > 0.0f
                ? glyph.AdvanceWidth * scale
                : glyphWidth;
        }

        return visibleWidth;
    }

    /// Resolves the horizontal offset required to place one line according to its authored text alignment.
    float Nintendo3DsStartupRenderManager2D::ResolveAlignedTextOffset(int32_t alignmentValue, int32_t boxWidth, float visibleWidth) const {
        if (boxWidth <= 0 || visibleWidth <= 0.0f) {
            return 0.0f;
        }

        const float availableWidth = static_cast<float>(boxWidth) - visibleWidth;
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
        viewportX = static_cast<int32_t>(std::round(viewport.X));
        int32_t resolvedViewportY = static_cast<int32_t>(std::round(viewport.Y));
        viewportWidth = std::max(static_cast<int32_t>(0), static_cast<int32_t>(std::round(viewport.Z)));
        viewportHeight = std::max(static_cast<int32_t>(0), static_cast<int32_t>(std::round(viewport.W)));
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
    void Nintendo3DsStartupRenderManager2D::RenderSprite(ISpriteDrawable2D* sprite) {
        if (sprite == nullptr || sprite->get_Parent() == nullptr) {
            AppendRenderTrace("Render2D.RenderSprite: sprite-or-parent-null");
            return;
        }

        float4 bounds = ResolveSpriteBounds(sprite);
        if (bounds.Z <= 0.0f || bounds.W <= 0.0f) {
            AppendRenderTrace("Render2D.RenderSprite: empty-bounds");
            return;
        }

        float3 scale = sprite->get_Parent()->get_Scale();
        float4 orientation = sprite->get_Parent()->get_Orientation();

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
        subTexture.width = static_cast<uint16_t>(std::max<int32_t>(1, static_cast<int32_t>(std::lround(sourceRect.Z * runtimeTexture->GetActualWidth()))));
        subTexture.height = static_cast<uint16_t>(std::max<int32_t>(1, static_cast<int32_t>(std::lround(sourceRect.W * runtimeTexture->GetActualHeight()))));
        subTexture.left = sourceRect.X * normalizedWidth;
        subTexture.right = subTexture.left + (sourceRect.Z * normalizedWidth);
        subTexture.top = 1.0f - (sourceRect.Y * normalizedHeight);
        subTexture.bottom = subTexture.top - (sourceRect.W * normalizedHeight);

        C2D_Image image;
        image.tex = runtimeTexture->GetNativeTexture();
        image.subtex = &subTexture;

        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, ConvertColor(sprite->get_Color()), 1.0f);
        C2D_SetTintMode(C2D_TintMult);

        C2D_DrawParams drawParams;
        drawParams.pos.w = bounds.Z * scale.X;
        drawParams.pos.h = bounds.W * scale.Y;
        drawParams.pos.x = bounds.X + static_cast<float>(ActiveViewportOffsetX) + (drawParams.pos.w * 0.5f);
        drawParams.pos.y = bounds.Y + static_cast<float>(ActiveViewportOffsetY) + (drawParams.pos.h * 0.5f);
        drawParams.center.x = drawParams.pos.w * 0.5f;
        drawParams.center.y = drawParams.pos.h * 0.5f;
        drawParams.depth = 0.0f;
        drawParams.angle = std::atan2(
            2.0 * (static_cast<double>(orientation.W) * static_cast<double>(orientation.Z)
                + static_cast<double>(orientation.X) * static_cast<double>(orientation.Y)),
            1.0 - 2.0 * (static_cast<double>(orientation.Y) * static_cast<double>(orientation.Y)
                + static_cast<double>(orientation.Z) * static_cast<double>(orientation.Z)));

        char spriteTraceMessage[256];
        std::snprintf(
            spriteTraceMessage,
            sizeof(spriteTraceMessage),
            "Render2D.RenderSprite: pos=(%.1f,%.1f) size=(%.1f,%.1f) scale=(%.2f,%.2f) angle=%.3f texture=%dx%d traceBytes=%u",
            drawParams.pos.x,
            drawParams.pos.y,
            drawParams.pos.w,
            drawParams.pos.h,
            scale.X,
            scale.Y,
            drawParams.angle,
            runtimeTexture->GetActualWidth(),
            runtimeTexture->GetActualHeight(),
            static_cast<unsigned>(runtimeTexture->GetDebugTraceSummary().size()));
        AppendDetailedRenderTrace(spriteTraceMessage);

        C2D_DrawImage(image, &drawParams, &tint);
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
        const float scale = ResolveTextScale(text);
        const float textureWidth = static_cast<float>(runtimeTexture->GetActualWidth());
        const float textureHeight = static_cast<float>(runtimeTexture->GetActualHeight());
        if (textureWidth <= 0.0f || textureHeight <= 0.0f) {
            AppendRenderTrace("Render2D.RenderText: invalid-font-texture-size");
            return;
        }

        const float lineHeight = std::max(font->get_LineHeight() * scale, 1.0f);
        const float spaceWidth = font->get_FontInfo() != nullptr
            ? std::max(font->get_FontInfo()->get_SpaceWidth() * scale, 1.0f)
            : std::max(lineHeight * 0.25f, 1.0f);
        std::vector<float> lineOffsets = BuildTextLineOffsets(text, font, content, scale, textureWidth);
        float renderOriginX = position.X + static_cast<float>(ActiveViewportOffsetX);
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        size_t lineIndex = 0;
        float lineOriginX = renderOriginX + (!lineOffsets.empty() ? lineOffsets[0] : 0.0f);
        char textTraceMessage[256];
        std::snprintf(
            textTraceMessage,
            sizeof(textTraceMessage),
            "Render2D.RenderText: pos=(%.1f,%.1f) scale=%.2f contentLength=%u traceBytes=%u",
            renderOriginX,
            position.Y + static_cast<float>(ActiveViewportOffsetY),
            scale,
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
                lineOriginX = renderOriginX + (lineIndex < lineOffsets.size() ? lineOffsets[lineIndex] : 0.0f);
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
            subTexture.width = static_cast<uint16_t>(std::max(1.0f, std::round(sourceRect.Z * textureWidth)));
            subTexture.height = static_cast<uint16_t>(std::max(1.0f, std::round(sourceRect.W * textureHeight)));
            subTexture.left = sourceRect.X * (textureWidth / static_cast<float>(runtimeTexture->GetPaddedWidth()));
            subTexture.right = subTexture.left + (sourceRect.Z * (textureWidth / static_cast<float>(runtimeTexture->GetPaddedWidth())));
            subTexture.top = 1.0f - (sourceRect.Y * (textureHeight / static_cast<float>(runtimeTexture->GetPaddedHeight())));
            subTexture.bottom = subTexture.top - (sourceRect.W * (textureHeight / static_cast<float>(runtimeTexture->GetPaddedHeight())));

            C2D_Image image;
            image.tex = runtimeTexture->GetNativeTexture();
            image.subtex = &subTexture;

            C2D_ImageTint tint;
            C2D_PlainImageTint(&tint, ConvertColor(text->get_Color()), 1.0f);

            C2D_DrawParams drawParams;
            drawParams.pos.x = lineOriginX + offsetX;
            drawParams.pos.y = position.Y + static_cast<float>(ActiveViewportOffsetY) + offsetY + (glyph.OffsetY * scale);
            drawParams.pos.w = std::max(1.0f, sourceRect.Z * textureWidth * scale);
            drawParams.pos.h = std::max(1.0f, sourceRect.W * textureHeight * scale);
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
                : drawParams.pos.w;
        }

        if (hasFirstGlyphTrace) {
            char glyphTraceMessage[512];
            std::snprintf(
                glyphTraceMessage,
                sizeof(glyphTraceMessage),
                "Render2D.RenderTextGlyphs: first='%c' src=(%.4f,%.4f,%.4f,%.4f) dst=(%.1f,%.1f,%.1f,%.1f) sub=(%.4f,%.4f,%.4f,%.4f) drawn=%d missing=%d",
                firstGlyphCharacter,
                firstGlyphSourceRect.X,
                firstGlyphSourceRect.Y,
                firstGlyphSourceRect.Z,
                firstGlyphSourceRect.W,
                firstGlyphDrawBounds.X,
                firstGlyphDrawBounds.Y,
                firstGlyphDrawBounds.Z,
                firstGlyphDrawBounds.W,
                firstGlyphSubTextureBounds.X,
                firstGlyphSubTextureBounds.Y,
                firstGlyphSubTextureBounds.Z,
                firstGlyphSubTextureBounds.W,
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

        byte4 fillColor = shape->get_FillColor();
        byte4 baseColor = shape->get_Color();
        u32 resolvedFillColor = fillColor.W > 0 ? ConvertColor(fillColor) : ConvertColor(baseColor);
        C2D_DrawRectSolid(
            position.X + static_cast<float>(ActiveViewportOffsetX),
            position.Y + static_cast<float>(ActiveViewportOffsetY),
            0.0f,
            static_cast<float>(size.X),
            static_cast<float>(size.Y),
            resolvedFillColor);

        if (shape->get_BorderThickness() <= 0.0f || shape->get_BorderColor().W == 0) {
            return;
        }

        const float borderThickness = std::max(1.0f, static_cast<float>(std::lround(shape->get_BorderThickness())));
        const float width = static_cast<float>(size.X);
        const float height = static_cast<float>(size.Y);
        const u32 borderColor = ConvertColor(shape->get_BorderColor());
        float viewportOriginX = position.X + static_cast<float>(ActiveViewportOffsetX);
        float viewportOriginY = position.Y + static_cast<float>(ActiveViewportOffsetY);
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
