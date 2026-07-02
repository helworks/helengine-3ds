namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Audits the Nintendo 3DS 3D renderer source so generated-core mesh drawing stays wired to one real citro3d-backed render manager.
/// </summary>
public class Nintendo3DsRenderManager3DSourceAuditTests {
    /// <summary>
    /// Verifies the Nintendo 3DS 3D renderer exposes the queue-capture and present-time playback seams required by the boot host.
    /// </summary>
    [Fact]
    public void Header_whenReal3dRendererIsDeclared_exposesQueueCaptureAndPlaybackSeams() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string headerPath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRenderManager3D.hpp");
        string headerCode = File.ReadAllText(headerPath);

        Assert.Contains("#include \"IDrawable3D.hpp\"", headerCode, StringComparison.Ordinal);
        Assert.Contains("#include \"IRenderVisitor3D.hpp\"", headerCode, StringComparison.Ordinal);
        Assert.Contains("#include \"RenderManager3D.hpp\"", headerCode, StringComparison.Ordinal);
        Assert.Contains("class Nintendo3DsRenderManager3D final : public RenderManager3D, public IRenderVisitor3D", headerCode, StringComparison.Ordinal);
        Assert.Contains("Nintendo3DsRenderManager3D();", headerCode, StringComparison.Ordinal);
        Assert.Contains("RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;", headerCode, StringComparison.Ordinal);
        Assert.Contains("RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath) override;", headerCode, StringComparison.Ordinal);
        Assert.Contains("void Draw() override;", headerCode, StringComparison.Ordinal);
        Assert.Contains("void Visit(IDrawable3D* drawable) override;", headerCode, StringComparison.Ordinal);
        Assert.Contains("void BeginFrame();", headerCode, StringComparison.Ordinal);
        Assert.Contains("void RenderTopScreen(C3D_RenderTarget* target, u32 clearColor);", headerCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS build wires `.v.pica` shaders into generated headers and object files so the 3D renderer can bind native shader binaries.
    /// </summary>
    [Fact]
    public void Makefile_when3dRendererUsesShaders_exportsPicaShaderBuildInputs() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string makefilePath = Path.Combine(repositoryRootPath, "Makefile");
        string makefileCode = File.ReadAllText(makefilePath);

        Assert.Contains("PICAFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))", makefileCode, StringComparison.Ordinal);
        Assert.Contains("$(PICAFILES:.v.pica=.shbin.o)", makefileCode, StringComparison.Ordinal);
        Assert.Contains("export HFILES := $(PICAFILES:.v.pica=_shbin.h)", makefileCode, StringComparison.Ordinal);
        Assert.Contains("$(OFILES_SOURCES) : $(HFILES)", makefileCode, StringComparison.Ordinal);
        Assert.Contains("%.shbin.o %_shbin.h : %.shbin", makefileCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS runtime-model build path expands indexed mesh positions into a platform-owned triangle stream for citro3d submission.
    /// </summary>
    [Fact]
    public void Source_whenModelIsBuiltFromRaw_expandsIndexedPositionsIntoPlatformOwnedTriangleStream() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string rendererSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRenderManager3D.cpp");
        string rendererSourceCode = File.ReadAllText(rendererSourcePath);
        string rendererHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRenderManager3D.hpp");
        string rendererHeaderCode = File.ReadAllText(rendererHeaderPath);
        string runtimeModelHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRuntimeModel.hpp");
        string runtimeModelHeaderCode = File.ReadAllText(runtimeModelHeaderPath);

        Assert.Contains("#include \"ModelAssetIndexData.hpp\"", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("::ModelAssetIndexData* indexData = ::ModelAssetIndexData::Resolve(data);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("std::vector<::float3> expandedPositions;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("expandedPositions.push_back((*data->Positions)[sourceIndex]);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("float3* expandedVertexData = static_cast<float3*>(linearAlloc(sizeof(float3) * expandedPositions.size()));", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("std::memcpy(expandedVertexData, expandedPositions.data(), sizeof(float3) * expandedPositions.size());", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("GSPGPU_FlushDataCache(expandedVertexData, sizeof(float3) * expandedPositions.size());", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("new Nintendo3DsRuntimeModel(expandedVertexData, static_cast<int32_t>(expandedPositions.size()))", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("float3* GetVertexData() const;", runtimeModelHeaderCode, StringComparison.Ordinal);
        Assert.Contains("int32_t GetVertexCount() const;", runtimeModelHeaderCode, StringComparison.Ordinal);
        Assert.Contains("void ReleaseModel(RuntimeModel* model) override;", rendererHeaderCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS renderer initializes one citro3d shader program and submits solid-color triangle draws through the active top-screen pass.
    /// </summary>
    [Fact]
    public void Source_whenTopScreenFrameIsRendered_bindsShaderProgramUploadsProjectionAndModelViewAndDrawsTriangles() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string rendererSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRenderManager3D.cpp");
        string rendererSourceCode = File.ReadAllText(rendererSourcePath);

        Assert.Contains("#include \"solid_color_shbin.h\"", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("DVLB_ParseFile((u32*)solid_color_shbin, solid_color_shbin_size);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("shaderProgramInit(&Program);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("shaderProgramSetVsh(&Program, &VertexShaderDvlb->DVLE[0]);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("UniformLocationProjection = shaderInstanceGetUniformLocation(Program.vertexShader, \"projection\");", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("UniformLocationModelView = shaderInstanceGetUniformLocation(Program.vertexShader, \"modelView\");", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("AttrInfo_AddFixed(attrInfo, 1);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_FixedAttribSet(1, 1.0f, 1.0f, 1.0f, 1.0f);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_CullFace(GPU_CULL_NONE);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_FrameDrawOn(target);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("BufInfo_Add(bufInfo, vertexData, sizeof(float3), 1, 0x0);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("GSPGPU_FlushDataCache(vertexData, sizeof(float3) * static_cast<uint32_t>(submittedVertexCount));", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("Mtx_PerspTilt(", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("&ProjectionMatrix,", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("Nintendo3DsPerspectiveFieldOfViewRadians,", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_AspectRatioTop,", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UniformLocationProjection, &ProjectionMatrix);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UniformLocationModelView, &gpuModelView);", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("C3D_DrawArrays(GPU_TRIANGLES, submesh->get_IndexStart(), submesh->get_IndexCount());", rendererSourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the Nintendo 3DS matrix upload helper transposes HelEngine row-major matrices into citro3d row vectors during GPU uniform upload.
    /// </summary>
    [Fact]
    public void Source_whenGpuMatrixIsBuilt_transposesHelEngineRowsIntoCitro3dRowVectors() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string rendererSourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRenderManager3D.cpp");
        string rendererSourceCode = File.ReadAllText(rendererSourcePath);
        string rendererHeaderPath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsRenderManager3D.hpp");
        string rendererHeaderCode = File.ReadAllText(rendererHeaderPath);

        Assert.Contains("gpuModelView.r[0].x = matrix.M11;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[0].y = matrix.M21;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[0].z = matrix.M31;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[0].w = matrix.M41;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[1].x = matrix.M12;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[1].y = matrix.M22;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[1].z = matrix.M32;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[1].w = matrix.M42;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[2].x = matrix.M13;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[2].y = matrix.M23;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[2].z = matrix.M33;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[2].w = matrix.M43;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[3].x = matrix.M14;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[3].y = matrix.M24;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[3].z = matrix.M34;", rendererSourceCode, StringComparison.Ordinal);
        Assert.Contains("gpuModelView.r[3].w = matrix.M44;", rendererSourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("float4x4 BuildProjectionMatrix(float nearPlaneDistance, float farPlaneDistance) const;", rendererHeaderCode, StringComparison.Ordinal);
    }
}
