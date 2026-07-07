#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <cstdint>
#include <string>
#include <vector>

#include <citro2d.h>

#include "byte4.hpp"
#include "float4.hpp"
#include "IRenderVisitor2D.hpp"
#include "RenderManager2D.hpp"

class ICamera;

namespace helengine::nintendo3ds {
    /// Captures and renders the generated-core 2D queue through a minimal citro2d top-screen pass.
    class Nintendo3DsStartupRenderManager2D : public RenderManager2D, public IRenderVisitor2D {
    public:
        /// Creates the Nintendo 3DS startup 2D renderer for startup-scene queue capture and citro2d playback.
        Nintendo3DsStartupRenderManager2D();

        /// Releases resources owned by the startup 2D renderer.
        ~Nintendo3DsStartupRenderManager2D() override;

        /// Builds one managed runtime texture that mirrors the supplied raw texture dimensions.
        /// <param name="data">Raw texture data requested by the runtime scene loader.</param>
        /// <returns>Managed runtime texture with matching dimensions.</returns>
        RuntimeTexture* BuildTextureFromRaw(TextureAsset* data) override;

        /// Builds one Nintendo 3DS runtime texture for cooked texture requests during startup-scene materialization.
        /// <param name="cookedAssetPath">Cooked texture asset path requested by the runtime scene loader.</param>
        /// <returns>Uploaded Nintendo 3DS runtime texture.</returns>
        RuntimeTexture* BuildTextureFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Captures one sprite draw request in generated-core render order for later rendering.
        /// <param name="sprite">Sprite draw request issued by runtime code.</param>
        void DrawSprite(ISpriteDrawable2D* sprite) override;

        /// Captures one text draw request in generated-core render order for later rendering.
        /// <param name="text">Text draw request issued by runtime code.</param>
        void DrawText(ITextDrawable2D* text) override;

        /// Captures one rounded-rectangle draw request in generated-core render order for later rendering.
        /// <param name="shape">Rounded-rectangle draw request issued by runtime code.</param>
        void DrawRoundedRect(IRoundedRectDrawable2D* shape) override;

        /// Walks the active generated-core camera 2D queue and lets each drawable submit itself into the frame capture.
        void Draw() override;

        /// Visits one ordered 2D drawable from the active generated-core camera queue.
        /// <param name="drawable">Ordered drawable visited from the active camera queue.</param>
        void Visit(IDrawable2D* drawable) override;

        /// Clears the previous frame's captured 2D draw requests before the next generated-core update begins.
        void BeginFrame();

        /// Draws the captured startup-scene 2D content onto the active top-screen citro2d scene.
        void RenderTopScreen();

        /// Draws the captured startup-scene 2D content onto the active bottom-screen citro2d scene.
        void RenderBottomScreen();

        /// Resolves the active top-screen clear color from the first enabled generated-core camera that requests one.
        /// <param name="fallbackColor">Fallback clear color used when no enabled camera publishes one.</param>
        /// <returns>Resolved top-screen clear color for the active frame.</returns>
        u32 ResolveTopScreenClearColor(u32 fallbackColor) const;

        /// Resolves the active bottom-screen clear color from the first enabled generated-core camera that requests one.
        /// <param name="fallbackColor">Fallback clear color used when no enabled camera publishes one.</param>
        /// <returns>Resolved bottom-screen clear color for the active frame.</returns>
        u32 ResolveBottomScreenClearColor(u32 fallbackColor) const;

    private:
        /// Identifies the Nintendo 3DS physical screen targeted by one captured 2D draw command.
        enum class Nintendo3DsScreenTarget {
            /// Stores one top-screen draw command.
            Top,

            /// Stores one bottom-screen draw command.
            Bottom
        };

        /// Identifies the concrete generated-core drawable kind stored in one captured draw command.
        enum class DrawCommandType {
            /// Stores one sprite draw request.
            Sprite,

            /// Stores one text draw request.
            Text,

            /// Stores one rounded-rectangle draw request.
            RoundedRect
        };

        /// Stores one captured generated-core 2D draw request together with its concrete drawable kind.
        struct DrawCommand {
            /// Stores the drawable kind that should be rendered.
            DrawCommandType Type;

            /// Stores which physical screen should receive the draw command.
            Nintendo3DsScreenTarget ScreenTarget;

            /// Stores the screen-local viewport X offset that should be applied during playback.
            int32_t ViewportOffsetX;

            /// Stores the screen-local viewport Y offset that should be applied during playback.
            int32_t ViewportOffsetY;

            /// Stores the generated-core drawable pointer to render.
            IDrawable2D* Drawable;
        };

        /// Converts one generated-core byte color into the citro2d RGBA layout expected by the Nintendo 3DS GPU.
        u32 ConvertColor(byte4 color) const;

        /// Resolves one sprite draw size from authored size overrides or runtime texture dimensions.
        float4 ResolveSpriteBounds(ISpriteDrawable2D* sprite) const;

        /// Resolves one text string using generated-core wrapping rules when authored wrapping is enabled.
        std::string ResolveTextContent(ITextDrawable2D* text) const;

        /// Resolves one generated-core font scale for Nintendo 3DS text glyph placement.
        float ResolveTextScale(ITextDrawable2D* text) const;

        /// Builds one horizontal offset per rendered text line so authored alignment is respected across wrapped and non-wrapped content.
        /// <param name="text">Text drawable that owns the authored layout box.</param>
        /// <param name="font">Font asset used to render the text.</param>
        /// <param name="content">Final rendered text content after wrapping has been applied.</param>
        /// <param name="scale">Resolved glyph scale.</param>
        /// <param name="textureWidth">Font-atlas texture width used to resolve normalized glyph bounds.</param>
        /// <returns>One horizontal offset per rendered line.</returns>
        std::vector<float> BuildTextLineOffsets(ITextDrawable2D* text, FontAsset* font, const std::string& content, float scale, float textureWidth) const;

        /// Measures the visible glyph width of one rendered text line.
        /// <param name="line">Single rendered line to measure.</param>
        /// <param name="font">Font asset that supplies glyph metrics.</param>
        /// <param name="scale">Resolved glyph scale.</param>
        /// <param name="textureWidth">Font-atlas texture width used to resolve normalized glyph bounds.</param>
        /// <returns>Visible line width in pixels.</returns>
        float MeasureVisibleTextLineWidth(const std::string& line, FontAsset* font, float scale, float textureWidth) const;

        /// Resolves the horizontal offset required to place one line according to its authored text alignment.
        /// <param name="alignmentValue">Horizontal alignment requested by the text drawable, encoded as its generated enum backing value.</param>
        /// <param name="boxWidth">Authored layout width available to the line.</param>
        /// <param name="visibleWidth">Measured visible glyph width of the line.</param>
        /// <returns>Horizontal offset that should be added before drawing the line.</returns>
        float ResolveAlignedTextOffset(int32_t alignmentValue, int32_t boxWidth, float visibleWidth) const;

        /// Resolves one authored camera viewport into Nintendo 3DS pixel-space bounds.
        /// <param name="camera">Runtime camera providing the authored viewport.</param>
        /// <returns>Viewport rectangle expressed in Nintendo 3DS pixel-space coordinates.</returns>
        float4 ResolveCameraViewport(ICamera* camera) const;

        /// Resolves the target screen and screen-local viewport offsets for one Nintendo 3DS camera viewport.
        /// <param name="viewport">Viewport rectangle expressed in Nintendo 3DS pixel-space coordinates.</param>
        /// <param name="screenTarget">Receives which physical screen should present the camera.</param>
        /// <param name="viewportX">Receives the screen-local viewport X offset.</param>
        /// <param name="viewportY">Receives the screen-local viewport Y offset.</param>
        /// <param name="viewportWidth">Receives the viewport width in pixels.</param>
        /// <param name="viewportHeight">Receives the viewport height in pixels.</param>
        void ResolveViewportTarget(
            const float4& viewport,
            Nintendo3DsScreenTarget& screenTarget,
            int32_t& viewportX,
            int32_t& viewportY,
            int32_t& viewportWidth,
            int32_t& viewportHeight) const;

        /// Renders one captured sprite draw request onto the active top-screen citro2d scene.
        void RenderSprite(ISpriteDrawable2D* sprite);

        /// Renders one captured text draw request onto the active top-screen citro2d scene.
        void RenderText(ITextDrawable2D* text);

        /// Renders one captured rounded-rectangle draw request onto the active top-screen citro2d scene.
        void RenderRoundedRect(IRoundedRectDrawable2D* shape);

        /// Stores the active screen targeted while one runtime camera queue is being captured.
        Nintendo3DsScreenTarget ActiveScreenTarget;

        /// Stores the active screen-local viewport X offset used during capture and playback.
        int32_t ActiveViewportOffsetX;

        /// Stores the active screen-local viewport Y offset used during capture and playback.
        int32_t ActiveViewportOffsetY;

        /// Stores whether the current frame captured one explicit top-screen clear color.
        bool HasTopScreenClearColor;

        /// Stores whether the current frame captured one explicit bottom-screen clear color.
        bool HasBottomScreenClearColor;

        /// Stores the current frame's resolved top-screen clear color.
        u32 TopScreenClearColor;

        /// Stores the current frame's resolved bottom-screen clear color.
        u32 BottomScreenClearColor;

        /// Stores the generated-core 2D draw requests captured in authored render order for the current frame.
        std::vector<DrawCommand> DrawCommands;
    };
}

#endif
