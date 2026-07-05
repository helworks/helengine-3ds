namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Audits the Nintendo 3DS startup 2D renderer source so sprite rendering stays wired to cooked texture deserialization and citro2d image drawing.
/// </summary>
public class Nintendo3DsStartupRenderManager2DSourceAuditTests {
    /// <summary>
    /// Verifies cooked texture requests deserialize generic texture assets before they are materialized into Nintendo 3DS runtime textures.
    /// </summary>
    [Fact]
    public void Source_whenCookedTextureIsRequested_deserializesTextureAssetBeforeBuildingRuntimeTexture() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include \"AssetSerializer.hpp\"", sourceCode, StringComparison.Ordinal);
        Assert.Contains("stream = ::File::OpenRead(cookedAssetPath);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("asset = ::AssetSerializer::Deserialize(stream);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("::TextureAsset* cookedTextureAsset = he_cpp_try_cast<TextureAsset>(asset);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("RuntimeTexture* runtimeTexture = BuildTextureFromRaw(cookedTextureAsset);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies sprite rendering issues one real citro2d textured-image draw instead of the previous solid-rectangle placeholder.
    /// </summary>
    [Fact]
    public void Source_whenSpriteIsRendered_drawsTexturedCitro2dImage() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string rendererSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
        string rendererSourceCode = File.ReadAllText(rendererSourcePath);
        string runtimeTextureSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRuntimeTexture.cpp");
        string runtimeTextureSourceCode = File.ReadAllText(runtimeTextureSourcePath);

        Assert.Contains("C3D_SyncDisplayTransfer(", runtimeTextureSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_TexInit(", runtimeTextureSourceCode, StringComparison.Ordinal);
        Assert.Contains("C2D_SetTintMode(C2D_TintMult);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C2D_DrawImage(", rendererSourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies sprite rendering applies the entity world scale and Z rotation so authored 2D logo animation survives Nintendo 3DS playback.
    /// </summary>
    [Fact]
    public void Source_whenSpriteIsRendered_appliesEntityScaleAndRotationToDrawParams() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string rendererSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
        string rendererSourceCode = File.ReadAllText(rendererSourcePath);

        Assert.Contains("float3 scale = sprite->get_Parent()->get_Scale();", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("float4 orientation = sprite->get_Parent()->get_Orientation();", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("drawParams.pos.w = bounds.Z * scale.X;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("drawParams.pos.h = bounds.W * scale.Y;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("drawParams.pos.x = bounds.X + static_cast<float>(ActiveViewportOffsetX) + (drawParams.pos.w * 0.5f);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("drawParams.pos.y = bounds.Y + static_cast<float>(ActiveViewportOffsetY) + (drawParams.pos.h * 0.5f);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("drawParams.center.x = drawParams.pos.w * 0.5f;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("drawParams.center.y = drawParams.pos.h * 0.5f;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("drawParams.angle = std::atan2(", rendererSourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies text rendering uses the cooked font atlas, glyph metrics, and authored alignment instead of the citro2d system-font parser.
    /// </summary>
    [Fact]
    public void Source_whenTextIsRendered_usesFontAtlasGlyphMetricsAndAlignment() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string rendererSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
        string rendererSourceCode = File.ReadAllText(rendererSourcePath);

        Assert.Contains("FontChar glyph;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("font->get_Texture()", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("glyph.OffsetY", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("glyph.AdvanceWidth", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("text->get_Alignment()", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("case TextAlignment::Center:", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("case TextAlignment::Right:", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("MeasureVisibleTextLineWidth(", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("ResolveAlignedTextOffset(", rendererSourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("font->MeasureString(content)", rendererSourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("C2D_TextParse(", rendererSourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies RGBA4444 cooked textures stay in their native 4-bit GPU format instead of being expanded into one guessed RGBA8 upload layout.
    /// </summary>
    [Fact]
    public void Source_whenRgba4444TextureIsUploaded_usesNativeRgba4GpuPath() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string runtimeTextureSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRuntimeTexture.cpp");
        string runtimeTextureSourceCode = File.ReadAllText(runtimeTextureSourcePath);

        Assert.Contains("GPU_RGBA4", runtimeTextureSourceCode, StringComparison.Ordinal);
        Assert.Contains("GX_TRANSFER_FMT_RGBA4", runtimeTextureSourceCode, StringComparison.Ordinal);
        Assert.Contains("PackNativeRgba4", runtimeTextureSourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies RGBA8 uploads are packed into the native Nintendo 3DS byte layout instead of writing shared-engine RGBA bytes directly.
    /// </summary>
    [Fact]
    public void Source_whenRgba32TextureIsUploaded_packsBytesForNativeRgba8Layout() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string runtimeTextureSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRuntimeTexture.cpp");
        string runtimeTextureSourceCode = File.ReadAllText(runtimeTextureSourcePath);

        Assert.Contains("uploadBuffer[destinationIndex] = decodedColor.W;", runtimeTextureSourceCode, StringComparison.Ordinal);
        Assert.Contains("uploadBuffer[destinationIndex + 1] = decodedColor.Z;", runtimeTextureSourceCode, StringComparison.Ordinal);
        Assert.Contains("uploadBuffer[destinationIndex + 2] = decodedColor.Y;", runtimeTextureSourceCode, StringComparison.Ordinal);
        Assert.Contains("uploadBuffer[destinationIndex + 3] = decodedColor.X;", runtimeTextureSourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS startup 2D renderer resolves camera viewports and routes draw commands to top or bottom screen targets.
    /// </summary>
    [Fact]
    public void Source_whenDrawingScene_visitsAllCamerasAndRoutesCommandsByViewportTarget() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ResolveCameraViewport(", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ResolveViewportTarget(", sourceCode, StringComparison.Ordinal);
        Assert.Contains("RenderBottomScreen()", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("renderQueue->VisitOrdered(this);\r\n            return;", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("renderQueue->VisitOrdered(this);\n            return;", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS startup 2D renderer rejects cameras that span from the top screen into the bottom screen.
    /// </summary>
    [Fact]
    public void Source_whenViewportSpansScreenBands_rejectsCrossScreenCameras() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("Nintendo 3DS 2D cameras must not span from the top screen into the bottom screen.", sourceCode, StringComparison.Ordinal);
        Assert.Contains("Nintendo 3DS 2D cameras must stay fully inside the bottom screen.", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies normalized camera viewport conversion follows the DS screen-band contract instead of mapping height across a stacked 3DS virtual surface.
    /// </summary>
    [Fact]
    public void Source_whenViewportIsNormalized_usesPerScreenDimensions() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupRenderManager2D.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("const bool targetsBottomScreen = viewport.Y >= 1.0f;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("const float targetScreenWidth = targetsBottomScreen", sourceCode, StringComparison.Ordinal);
        Assert.Contains("viewport.Y * Nintendo3DsTopScreenHeight", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("viewport.Y * (Nintendo3DsTopScreenHeight + Nintendo3DsBottomScreenHeight)", sourceCode, StringComparison.Ordinal);
    }
}
