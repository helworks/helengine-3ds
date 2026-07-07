#include "platform/3ds/Nintendo3DsRenderManager3D.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <3ds.h>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "AmbientLightComponent.hpp"
#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "Core.hpp"
#include "DirectionalLightComponent.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "IRenderQueue3D.hpp"
#include "MeshComponent.hpp"
#include "ModelAsset.hpp"
#include "ModelAssetIndexData.hpp"
#include "ModelSubmeshResolver.hpp"
#include "ObjectManager.hpp"
#include "PlatformMaterialAsset.hpp"
#include "RenderTarget.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeMaterialLightingModel.hpp"
#include "RuntimeSubmesh.hpp"
#include "SceneManager.hpp"
#include "TextureAsset.hpp"
#include "float2.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "platform/3ds/Nintendo3DsModelVertex.hpp"
#include "platform/3ds/Nintendo3DsRuntimeMaterial.hpp"
#include "platform/3ds/Nintendo3DsRuntimeModel.hpp"
#include "platform/3ds/Nintendo3DsRuntimeTexture.hpp"
#include "runtime/native_cast.hpp"
#include "lit_color_shbin.h"
#include "lit_color_shbin.h"
#include "lit_textured_shbin.h"
#include "system/io/file.hpp"

namespace helengine::nintendo3ds {
    namespace {
        /// Stores the Nintendo 3DS top-screen width in pixels.
        constexpr int32_t Nintendo3DsTopScreenWidth = 400;

        /// Stores the Nintendo 3DS top-screen height in pixels.
        constexpr int32_t Nintendo3DsTopScreenHeight = 240;

        /// Stores the minimum safe near-plane distance for the Nintendo 3DS lit-color path.
        constexpr float Nintendo3DsMinimumNearPlaneDistance = 0.01f;

        /// Stores the minimum safe separation between near and far planes for the Nintendo 3DS lit-color path.
        constexpr float Nintendo3DsMinimumPlaneSeparation = 0.01f;

        /// Stores the fixed field-of-view used by the first Nintendo 3DS solid-color renderer milestone.
        constexpr float Nintendo3DsPerspectiveFieldOfViewRadians = 0.78539816339744831f;

        static_assert(sizeof(Nintendo3DsModelVertex) == sizeof(float) * 8, "Nintendo 3DS model vertices must stay tightly packed for citro3d attribute submission.");

        /// Stores the shared SD-card trace path used by Nintendo 3DS renderer diagnostics.
        constexpr const char* Nintendo3DsRenderTracePath = "sdmc:/helengine_3ds_render_trace.txt";

        /// Stores how many 3D frame trace samples remain before the renderer stops appending diagnostics.
        int Nintendo3DsRender3DTraceFramesRemaining = 24;

        /// Stores how many detailed 3D draw-call trace lines remain before per-draw diagnostics stop appending.
        int Nintendo3DsRender3DDetailLinesRemaining = 64;

        /// Stores how many transform-level 3D diagnostics remain before camera and object transform summaries stop appending.
        int Nintendo3DsRender3DTransformLinesRemaining = 20;

        /// Stores the last scene-signature string observed by the 3D renderer trace budget so scene transitions can re-arm diagnostics.
        std::string Nintendo3DsLastRenderSceneSignature;

        /// Appends one diagnostic line to the shared Nintendo 3DS renderer trace file.
        /// <param name="message">Trace line that describes one 3D renderer boundary.</param>
        void AppendRenderTrace(const char* message) {
            if (message == nullptr || Nintendo3DsRender3DTraceFramesRemaining <= 0) {
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
            if (message == nullptr || Nintendo3DsRender3DDetailLinesRemaining <= 0) {
                return;
            }

            Nintendo3DsRender3DDetailLinesRemaining--;
            AppendRenderTrace(message);
        }

        /// Appends one transform-level diagnostic line while the transform budget remains available.
        /// <param name="message">Transform summary payload.</param>
        void AppendTransformRenderTrace(const char* message) {
            if (message == nullptr || Nintendo3DsRender3DTransformLinesRemaining <= 0) {
                return;
            }

            Nintendo3DsRender3DTransformLinesRemaining--;
            AppendRenderTrace(message);
        }

        /// Re-arms the renderer trace budgets whenever the loaded-scene signature changes so post-transition frames stay observable.
        /// <param name="core">Generated-core instance that exposes the current loaded-scene list.</param>
        void ResetRenderTraceBudgetForSceneTransitions(Core* core) {
            if (core == nullptr || core->get_SceneManager() == nullptr) {
                return;
            }

            List<std::string>* loadedSceneIds = core->get_SceneManager()->GetLoadedSceneIds();
            if (loadedSceneIds == nullptr) {
                return;
            }

            std::string sceneSignature;
            for (int32_t index = 0; index < loadedSceneIds->get_Count(); index++) {
                if (index > 0) {
                    sceneSignature += "|";
                }

                sceneSignature += (*loadedSceneIds).get_Item(index);
            }

            delete loadedSceneIds;

            if (sceneSignature == Nintendo3DsLastRenderSceneSignature) {
                return;
            }

            Nintendo3DsLastRenderSceneSignature = sceneSignature;
            Nintendo3DsRender3DTraceFramesRemaining = 24;
            Nintendo3DsRender3DDetailLinesRemaining = 64;
            Nintendo3DsRender3DTransformLinesRemaining = 20;

            std::FILE* file = std::fopen(Nintendo3DsRenderTracePath, "w");
            if (file != nullptr) {
                std::fprintf(file, "Render3D.SceneTransition: loadedScenes=%s\n", sceneSignature.c_str());
                std::fclose(file);
            }
        }

    }

    /// Creates the Nintendo 3DS 3D renderer for startup-scene materialization and queued 3D playback.
    Nintendo3DsRenderManager3D::Nintendo3DsRenderManager3D()
        : CapturedCameras()
        , ProjectionMatrix()
        , ActiveViewMatrix(float4x4::get_Identity())
        , ActiveCameraOrientation(0.0f, 0.0f, 0.0f, 1.0f)
        , ActiveTarget(nullptr)
        , ActiveCapturedDrawables(nullptr)
        , VertexShaderDvlb(nullptr)
        , Program()
        , HasShaderProgram(false)
        , TexturedVertexShaderDvlb(nullptr)
        , TexturedProgram()
        , HasTexturedShaderProgram(false)
        , UniformLocationProjection(-1)
        , UniformLocationModelView(-1)
        , UniformLocationLightVector(-1)
        , UniformLocationLightColor(-1)
        , UniformLocationAmbientColor(-1)
        , UniformLocationBaseColor(-1)
        , TexturedUniformLocationProjection(-1)
        , TexturedUniformLocationModelView(-1)
        , TexturedUniformLocationLightVector(-1)
        , TexturedUniformLocationLightColor(-1)
        , TexturedUniformLocationAmbientColor(-1)
        , TexturedUniformLocationBaseColor(-1)
        , TopScreenClearColor(0)
        , HasTopScreenClearColor(false) {
    }

    /// Releases shader resources owned by the Nintendo 3DS 3D renderer.
    Nintendo3DsRenderManager3D::~Nintendo3DsRenderManager3D() {
        ReleaseShaderResources();
    }

    /// Builds one Nintendo 3DS runtime model from the supplied raw asset.
    RuntimeModel* Nintendo3DsRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("Nintendo 3DS model asset is required.");
        } else if (data->Positions == nullptr || data->Positions->Length == 0) {
            throw std::invalid_argument("Nintendo 3DS model data must include positions.");
        } else if (data->Normals == nullptr || data->Normals->Length != data->Positions->Length) {
            throw std::invalid_argument("Nintendo 3DS model data must include normals for every authored position.");
        }

        ::ModelAssetIndexData* indexData = ::ModelAssetIndexData::Resolve(data);
        const bool hasTextureCoordinates = data->TexCoords != nullptr && data->TexCoords->Length == data->Positions->Length;
        std::vector<Nintendo3DsModelVertex> expandedVertices;
        try {
            const bool hasIndices = indexData != nullptr && indexData->IndexCount > 0;
            expandedVertices.reserve(static_cast<std::size_t>(hasIndices ? indexData->IndexCount : data->Positions->Length));
            if (hasIndices && indexData->Uses32BitIndices && indexData->Indices32 != nullptr) {
                for (int32_t index = 0; index < indexData->Indices32->Length; index++) {
                    const uint32_t sourceIndex = (*indexData->Indices32)[index];
                    if (sourceIndex >= static_cast<uint32_t>(data->Positions->Length)) {
                        throw std::out_of_range("Nintendo 3DS model index exceeds the authored position range.");
                    }

                    Nintendo3DsModelVertex vertex;
                    vertex.PositionX = (*data->Positions)[sourceIndex].X;
                    vertex.PositionY = (*data->Positions)[sourceIndex].Y;
                    vertex.PositionZ = (*data->Positions)[sourceIndex].Z;
                    vertex.TextureCoordinateX = hasTextureCoordinates
                        ? (*data->TexCoords)[sourceIndex].X
                        : 0.0f;
                    vertex.TextureCoordinateY = hasTextureCoordinates
                        ? (*data->TexCoords)[sourceIndex].Y
                        : 0.0f;
                    vertex.NormalX = (*data->Normals)[sourceIndex].X;
                    vertex.NormalY = (*data->Normals)[sourceIndex].Y;
                    vertex.NormalZ = (*data->Normals)[sourceIndex].Z;
                    expandedVertices.push_back(vertex);
                }
            } else if (hasIndices && indexData->Indices16 != nullptr) {
                for (int32_t index = 0; index < indexData->Indices16->Length; index++) {
                    const uint16_t sourceIndex = (*indexData->Indices16)[index];
                    if (sourceIndex >= static_cast<uint16_t>(data->Positions->Length)) {
                        throw std::out_of_range("Nintendo 3DS model index exceeds the authored position range.");
                    }

                    Nintendo3DsModelVertex vertex;
                    vertex.PositionX = (*data->Positions)[sourceIndex].X;
                    vertex.PositionY = (*data->Positions)[sourceIndex].Y;
                    vertex.PositionZ = (*data->Positions)[sourceIndex].Z;
                    vertex.TextureCoordinateX = hasTextureCoordinates
                        ? (*data->TexCoords)[sourceIndex].X
                        : 0.0f;
                    vertex.TextureCoordinateY = hasTextureCoordinates
                        ? (*data->TexCoords)[sourceIndex].Y
                        : 0.0f;
                    vertex.NormalX = (*data->Normals)[sourceIndex].X;
                    vertex.NormalY = (*data->Normals)[sourceIndex].Y;
                    vertex.NormalZ = (*data->Normals)[sourceIndex].Z;
                    expandedVertices.push_back(vertex);
                }
            } else {
                for (int32_t index = 0; index < data->Positions->Length; index++) {
                    Nintendo3DsModelVertex vertex;
                    vertex.PositionX = (*data->Positions)[index].X;
                    vertex.PositionY = (*data->Positions)[index].Y;
                    vertex.PositionZ = (*data->Positions)[index].Z;
                    vertex.TextureCoordinateX = hasTextureCoordinates
                        ? (*data->TexCoords)[index].X
                        : 0.0f;
                    vertex.TextureCoordinateY = hasTextureCoordinates
                        ? (*data->TexCoords)[index].Y
                        : 0.0f;
                    vertex.NormalX = (*data->Normals)[index].X;
                    vertex.NormalY = (*data->Normals)[index].Y;
                    vertex.NormalZ = (*data->Normals)[index].Z;
                    expandedVertices.push_back(vertex);
                }
            }
        } catch (...) {
            delete indexData;
            throw;
        }

        delete indexData;

        Nintendo3DsModelVertex* expandedVertexData = static_cast<Nintendo3DsModelVertex*>(linearAlloc(sizeof(Nintendo3DsModelVertex) * expandedVertices.size()));
        if (expandedVertexData == nullptr) {
            throw std::bad_alloc();
        }

        std::memcpy(expandedVertexData, expandedVertices.data(), sizeof(Nintendo3DsModelVertex) * expandedVertices.size());
        GSPGPU_FlushDataCache(expandedVertexData, sizeof(Nintendo3DsModelVertex) * expandedVertices.size());
        Nintendo3DsRuntimeModel* runtimeModel = new Nintendo3DsRuntimeModel(expandedVertexData, static_cast<int32_t>(expandedVertices.size()));
        runtimeModel->SetBounds(data->BoundsMin, data->BoundsMax);
        runtimeModel->SetSubmeshes(ModelSubmeshResolver::BuildRuntimeSubmeshes(data));
        return runtimeModel;
    }

    /// Builds one Nintendo 3DS runtime model for cooked model requests during startup-scene materialization.
    RuntimeModel* Nintendo3DsRenderManager3D::BuildModelFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) {
        (void)contentStreamSource;
        if (cookedAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked model asset path is required.");
        }

        ::FileStream* stream = nullptr;
        ::Asset* asset = nullptr;
        try {
            stream = ::File::OpenRead(cookedAssetPath);
            asset = ::AssetSerializer::Deserialize(stream);
            delete stream;
            stream = nullptr;

            ::ModelAsset* cookedModelAsset = he_cpp_try_cast<ModelAsset>(asset);
            if (cookedModelAsset == nullptr) {
                throw std::invalid_argument("Nintendo 3DS cooked model payloads must deserialize as ModelAsset.");
            }

            RuntimeModel* runtimeModel = BuildModelFromRaw(cookedModelAsset);
            delete cookedModelAsset;
            return runtimeModel;
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

    /// Builds one lightweight runtime material from one platform-owned cooked material asset.
    RuntimeMaterial* Nintendo3DsRenderManager3D::BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw std::invalid_argument("Nintendo 3DS cooked material asset is required.");
        }

        Nintendo3DsRuntimeMaterial* runtimeMaterial = new Nintendo3DsRuntimeMaterial();
        runtimeMaterial->set_Id(materialAsset->get_Id());
        runtimeMaterial->set_LightingModel(materialAsset->Lit
            ? RuntimeMaterialLightingModel::MetalRoughPbr
            : RuntimeMaterialLightingModel::Unlit);
        runtimeMaterial->SetBaseColor(float4(
            static_cast<float>(materialAsset->BaseColorR) / 255.0f,
            static_cast<float>(materialAsset->BaseColorG) / 255.0f,
            static_cast<float>(materialAsset->BaseColorB) / 255.0f,
            static_cast<float>(materialAsset->BaseColorA) / 255.0f));
        runtimeMaterial->SetTextureRelativePath(materialAsset->TextureRelativePath);
        runtimeMaterial->set_SupportsNormalMapping(false);
        runtimeMaterial->set_SupportsEmissive(false);
        runtimeMaterial->set_CastsShadows(materialAsset->Lit);
        runtimeMaterial->set_ReceivesShadows(materialAsset->Lit);
        return runtimeMaterial;
    }

    /// Builds one lightweight runtime material from one cooked material asset path.
    RuntimeMaterial* Nintendo3DsRenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) {
        (void)contentStreamSource;
        if (cookedAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked material asset path is required.");
        }

        ::FileStream* stream = nullptr;
        ::Asset* asset = nullptr;
        try {
            stream = ::File::OpenRead(cookedAssetPath);
            asset = ::AssetSerializer::Deserialize(stream);
            delete stream;
            stream = nullptr;

            ::PlatformMaterialAsset* cookedMaterialAsset = he_cpp_try_cast<PlatformMaterialAsset>(asset);
            if (cookedMaterialAsset == nullptr) {
                throw std::invalid_argument("Nintendo 3DS cooked material payloads must deserialize as PlatformMaterialAsset.");
            }

            asset = nullptr;
            Nintendo3DsRuntimeMaterial* runtimeMaterial = static_cast<Nintendo3DsRuntimeMaterial*>(BuildMaterialFromCooked(cookedMaterialAsset));
            try {
                AttachCookedDiffuseTexture(runtimeMaterial, cookedMaterialAsset, cookedAssetPath);
            } catch (...) {
                delete runtimeMaterial;
                delete cookedMaterialAsset;
                throw;
            }

            delete cookedMaterialAsset;
            return runtimeMaterial;
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

    /// Builds one lightweight runtime material from one raw material asset path.
    RuntimeMaterial* Nintendo3DsRenderManager3D::BuildMaterialFromRawAsset(
        ContentManager* assetContentManager,
        std::string materialAssetPath) {
        if (assetContentManager == nullptr) {
            throw std::invalid_argument("Nintendo 3DS material content manager is required.");
        } else if (materialAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS material asset path is required.");
        }

        return BuildMaterialFromCooked(materialAssetPath, nullptr);
    }

    /// Releases one Nintendo 3DS runtime model after the final scene reference is removed.
    void Nintendo3DsRenderManager3D::ReleaseModel(RuntimeModel* model) {
        Nintendo3DsRuntimeModel* runtimeModel = dynamic_cast<Nintendo3DsRuntimeModel*>(model);
        if (runtimeModel == nullptr) {
            delete model;
            return;
        }

        runtimeModel->Dispose();
        delete runtimeModel;
    }

    /// Walks the active generated-core camera list and captures the top-screen cameras that should render during present-time playback.
    void Nintendo3DsRenderManager3D::Draw() {
        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            AppendRenderTrace("Render3D.Draw: core-or-object-manager-null");
            return;
        }

        ResetRenderTraceBudgetForSceneTransitions(core);

        List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
        if (cameras == nullptr) {
            AppendRenderTrace("Render3D.Draw: cameras-null");
            return;
        }

        if (Nintendo3DsRender3DTraceFramesRemaining > 0) {
            char message[128];
            std::snprintf(message, sizeof(message), "Render3D.Draw: cameraCount=%d", static_cast<int>(cameras->get_Count()));
            AppendRenderTrace(message);
        }

        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            ICamera* camera = (*cameras)[cameraIndex];
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            const bool isTopScreenCamera = IsTopScreenCamera(camera);
            if (Nintendo3DsRender3DTraceFramesRemaining > 0) {
                RenderTarget* renderTarget = camera->get_RenderTarget();
                IRenderQueue3D* renderQueue = camera->get_RenderQueue3D();
                float3 localPosition = camera->get_Parent()->get_LocalPosition();
                char message[320];
                std::snprintf(
                    message,
                    sizeof(message),
                    "Render3D.Draw: cameraIndex=%d top=%d camera=%p parent=%p queue=%p queueCount=%d far=%.3f localPos=(%.3f,%.3f,%.3f) target=%dx%d viewport=(%.3f,%.3f,%.3f,%.3f)",
                    static_cast<int>(cameraIndex),
                    isTopScreenCamera ? 1 : 0,
                    static_cast<void*>(camera),
                    static_cast<void*>(camera->get_Parent()),
                    static_cast<void*>(renderQueue),
                    renderQueue != nullptr ? static_cast<int>(renderQueue->get_Count()) : -1,
                    camera->get_FarPlaneDistance(),
                    localPosition.X,
                    localPosition.Y,
                    localPosition.Z,
                    renderTarget != nullptr ? static_cast<int>(renderTarget->get_Width()) : -1,
                    renderTarget != nullptr ? static_cast<int>(renderTarget->get_Height()) : -1,
                    camera->get_Viewport().X,
                    camera->get_Viewport().Y,
                    camera->get_Viewport().Z,
                    camera->get_Viewport().W);
                AppendRenderTrace(message);
            }

            if (!isTopScreenCamera) {
                continue;
            }

            IRenderQueue3D* renderQueue = camera->get_RenderQueue3D();
            CapturedCameraState cameraState;
            cameraState.ViewMatrix = BuildCameraViewMatrix(camera);
            cameraState.Orientation = camera->get_Parent()->get_Orientation();
            cameraState.Position = camera->get_Parent()->get_Position();
            cameraState.NearPlaneDistance = ResolveNearPlaneDistance(camera);
            cameraState.FarPlaneDistance = ResolveFarPlaneDistance(camera, cameraState.NearPlaneDistance);
            cameraState.QueueCount = renderQueue != nullptr ? renderQueue->get_Count() : 0;
            cameraState.QueueCapacity = renderQueue != nullptr ? renderQueue->get_Capacity() : 0;
            if (cameraState.QueueCount > 0) {
                cameraState.Drawables.reserve(static_cast<std::size_t>(cameraState.QueueCount));
            }

            if (renderQueue != nullptr) {
                ActiveCapturedDrawables = &cameraState.Drawables;
                renderQueue->VisitOrdered(this);
                ActiveCapturedDrawables = nullptr;
            }

            CapturedCameras.push_back(cameraState);
        }

    }

    /// Visits one ordered 3D drawable from the active generated-core camera queue and draws supported mesh content through the lit-color path.
    void Nintendo3DsRenderManager3D::Visit(IDrawable3D* drawable) {
        if (drawable == nullptr) {
            AppendDetailedRenderTrace("Render3D.Visit: drawable-null");
            return;
        } else if (drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            AppendDetailedRenderTrace("Render3D.Visit: drawable-parent-disabled");
            return;
        }

        if (ActiveCapturedDrawables != nullptr) {
            ActiveCapturedDrawables->push_back(drawable);
            return;
        }

        MeshComponent* meshComponent = he_cpp_try_cast<MeshComponent>(drawable);
        if (meshComponent == nullptr || meshComponent->get_Model() == nullptr) {
            AppendDetailedRenderTrace("Render3D.Visit: drawable-not-mesh-or-model-null");
            return;
        }

        Nintendo3DsRuntimeModel* runtimeModel = dynamic_cast<Nintendo3DsRuntimeModel*>(meshComponent->get_Model());
        if (runtimeModel == nullptr) {
            AppendDetailedRenderTrace("Render3D.Visit: runtime-model-cast-failed");
            return;
        }

        if (Nintendo3DsRender3DTraceFramesRemaining > 0) {
            char message[160];
            std::snprintf(
                message,
                sizeof(message),
                "Render3D.Visit: entity-active submeshCount=%d vertexCount=%d",
                runtimeModel->get_Submeshes() != nullptr ? static_cast<int>(runtimeModel->get_Submeshes()->Length) : -1,
                static_cast<int>(runtimeModel->GetVertexCount()));
            AppendDetailedRenderTrace(message);
        }

        DrawRuntimeModel(meshComponent, runtimeModel);
    }

    /// Clears the previous frame's captured 3D camera list before the next generated-core draw begins.
    void Nintendo3DsRenderManager3D::BeginFrame() {
        CapturedCameras.clear();
        ActiveViewMatrix = float4x4::get_Identity();
        ActiveCameraOrientation = float4(0.0f, 0.0f, 0.0f, 1.0f);
        ActiveTarget = nullptr;
        ActiveCapturedDrawables = nullptr;
        HasTopScreenClearColor = false;
        TopScreenClearColor = 0;
        if (Nintendo3DsRender3DTraceFramesRemaining > 0) {
            AppendRenderTrace("Render3D.BeginFrame");
        }
    }

    /// Replays the captured 3D frame state onto the supplied Nintendo 3DS render target.
    void Nintendo3DsRenderManager3D::RenderTopScreen(C3D_RenderTarget* target, u32 clearColor) {
        if (target == nullptr) {
            throw std::invalid_argument("Nintendo 3DS top-screen render target is required.");
        }

        ActiveTarget = target;
        C3D_RenderTargetClear(target, C3D_CLEAR_ALL, __builtin_bswap32(clearColor), 0);
        C3D_FrameDrawOn(target);
        if (CapturedCameras.empty()) {
            AppendRenderTrace("Render3D.RenderTopScreen: capturedCameras=0");
            ActiveTarget = nullptr;
            Nintendo3DsRender3DTraceFramesRemaining--;
            return;
        }

        if (Nintendo3DsRender3DTraceFramesRemaining > 0) {
            char message[128];
            std::snprintf(message, sizeof(message), "Render3D.RenderTopScreen: capturedCameras=%d", static_cast<int>(CapturedCameras.size()));
            AppendRenderTrace(message);
        }

        for (const CapturedCameraState& cameraState : CapturedCameras) {
            DrawCamera(cameraState);
        }

        ActiveTarget = nullptr;
        if (Nintendo3DsRender3DTraceFramesRemaining > 0) {
            Nintendo3DsRender3DTraceFramesRemaining--;
        }
    }

    /// Initializes the lit untextured shader program the first time one untextured 3D draw is submitted.
    void Nintendo3DsRenderManager3D::EnsureShaderInitialized() {
        if (HasShaderProgram) {
            return;
        }

        VertexShaderDvlb = DVLB_ParseFile((u32*)lit_color_shbin, lit_color_shbin_size);
        if (VertexShaderDvlb == nullptr) {
            throw std::runtime_error("Nintendo 3DS lit-color vertex shader parsing failed.");
        }

        shaderProgramInit(&Program);
        shaderProgramSetVsh(&Program, &VertexShaderDvlb->DVLE[0]);
        C3D_BindProgram(&Program);

        UniformLocationProjection = shaderInstanceGetUniformLocation(Program.vertexShader, "projection");
        UniformLocationModelView = shaderInstanceGetUniformLocation(Program.vertexShader, "modelView");
        UniformLocationLightVector = shaderInstanceGetUniformLocation(Program.vertexShader, "lightVec");
        UniformLocationLightColor = shaderInstanceGetUniformLocation(Program.vertexShader, "lightClr");
        UniformLocationAmbientColor = shaderInstanceGetUniformLocation(Program.vertexShader, "ambientClr");
        UniformLocationBaseColor = shaderInstanceGetUniformLocation(Program.vertexShader, "baseColor");

        HasShaderProgram = true;
    }

    /// Initializes the lit textured shader program the first time one textured 3D draw is submitted.
    void Nintendo3DsRenderManager3D::EnsureTexturedShaderInitialized() {
        if (HasTexturedShaderProgram) {
            return;
        }

        TexturedVertexShaderDvlb = DVLB_ParseFile((u32*)lit_textured_shbin, lit_textured_shbin_size);
        if (TexturedVertexShaderDvlb == nullptr) {
            throw std::runtime_error("Nintendo 3DS lit-textured vertex shader parsing failed.");
        }

        shaderProgramInit(&TexturedProgram);
        shaderProgramSetVsh(&TexturedProgram, &TexturedVertexShaderDvlb->DVLE[0]);
        C3D_BindProgram(&TexturedProgram);

        TexturedUniformLocationProjection = shaderInstanceGetUniformLocation(TexturedProgram.vertexShader, "projection");
        TexturedUniformLocationModelView = shaderInstanceGetUniformLocation(TexturedProgram.vertexShader, "modelView");
        TexturedUniformLocationLightVector = shaderInstanceGetUniformLocation(TexturedProgram.vertexShader, "lightVec");
        TexturedUniformLocationLightColor = shaderInstanceGetUniformLocation(TexturedProgram.vertexShader, "lightClr");
        TexturedUniformLocationAmbientColor = shaderInstanceGetUniformLocation(TexturedProgram.vertexShader, "ambientClr");
        TexturedUniformLocationBaseColor = shaderInstanceGetUniformLocation(TexturedProgram.vertexShader, "baseColor");

        HasTexturedShaderProgram = true;
    }

    /// Reapplies the fixed Citro3D pipeline state required by the lit untextured 3D path after Citro2D work may have changed GPU state.
    void Nintendo3DsRenderManager3D::ApplyUntexturedPipelineState() {
        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 3);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
        C3D_CullFace(GPU_CULL_NONE);
    }

    /// Reapplies the fixed Citro3D pipeline state required by the lit textured 3D path after Citro2D work may have changed GPU state.
    void Nintendo3DsRenderManager3D::ApplyTexturedPipelineState() {
        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
        AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);
        AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 3);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
        C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
        C3D_CullFace(GPU_CULL_NONE);
    }

    /// Uploads one shared set of projection, transform, lighting, and base-color uniforms to the currently bound shader program.
    void Nintendo3DsRenderManager3D::ApplyCommonLightingUniforms(
        int projectionLocation,
        int modelViewLocation,
        int lightVectorLocation,
        int lightColorLocation,
        int ambientColorLocation,
        int baseColorLocation,
        const C3D_Mtx& projectionMatrix,
        const C3D_Mtx& modelViewMatrix,
        const float3& viewSpaceLightVector,
        const float4& directionalLightColor,
        const float4& ambientLightColor,
        const float4& baseColor) {
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation, &projectionMatrix);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, modelViewLocation, &modelViewMatrix);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, lightVectorLocation, viewSpaceLightVector.X, viewSpaceLightVector.Y, viewSpaceLightVector.Z, 0.0f);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, lightColorLocation, directionalLightColor.X, directionalLightColor.Y, directionalLightColor.Z, directionalLightColor.W);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, ambientColorLocation, ambientLightColor.X, ambientLightColor.Y, ambientLightColor.Z, ambientLightColor.W);
        C3D_FVUnifSet(GPU_VERTEX_SHADER, baseColorLocation, baseColor.X, baseColor.Y, baseColor.Z, baseColor.W);
    }

    /// Releases the shader program and parsed shader binary when the renderer is destroyed.
    void Nintendo3DsRenderManager3D::ReleaseShaderResources() {
        if (HasShaderProgram) {
            shaderProgramFree(&Program);
            HasShaderProgram = false;
        }
        if (HasTexturedShaderProgram) {
            shaderProgramFree(&TexturedProgram);
            HasTexturedShaderProgram = false;
        }
        if (VertexShaderDvlb != nullptr) {
            DVLB_Free(VertexShaderDvlb);
            VertexShaderDvlb = nullptr;
        }
        if (TexturedVertexShaderDvlb != nullptr) {
            DVLB_Free(TexturedVertexShaderDvlb);
            TexturedVertexShaderDvlb = nullptr;
        }

        UniformLocationProjection = -1;
        UniformLocationModelView = -1;
        UniformLocationLightVector = -1;
        UniformLocationLightColor = -1;
        UniformLocationAmbientColor = -1;
        UniformLocationBaseColor = -1;
        TexturedUniformLocationProjection = -1;
        TexturedUniformLocationModelView = -1;
        TexturedUniformLocationLightVector = -1;
        TexturedUniformLocationLightColor = -1;
        TexturedUniformLocationAmbientColor = -1;
        TexturedUniformLocationBaseColor = -1;
    }

    /// Draws one captured camera through the lit-color top-screen path.
    void Nintendo3DsRenderManager3D::DrawCamera(const CapturedCameraState& cameraState) {
        if (Nintendo3DsRender3DTraceFramesRemaining > 0) {
            char message[160];
            std::snprintf(
                message,
                sizeof(message),
                "Render3D.DrawCamera: queueCount=%d queueCapacity=%d near=%.3f far=%.3f",
                static_cast<int>(cameraState.QueueCount),
                static_cast<int>(cameraState.QueueCapacity),
                cameraState.NearPlaneDistance,
                cameraState.FarPlaneDistance);
            AppendRenderTrace(message);
        }

        if (Nintendo3DsRender3DTransformLinesRemaining > 0) {
            char message[256];
            std::snprintf(
                message,
                sizeof(message),
                "Render3D.CameraTransform: pos=(%.3f,%.3f,%.3f) rot=(%.3f,%.3f,%.3f,%.3f) near=%.3f far=%.3f",
                cameraState.Position.X,
                cameraState.Position.Y,
                cameraState.Position.Z,
                cameraState.Orientation.X,
                cameraState.Orientation.Y,
                cameraState.Orientation.Z,
                cameraState.Orientation.W,
                cameraState.NearPlaneDistance,
                cameraState.FarPlaneDistance);
            AppendTransformRenderTrace(message);
        }

        Mtx_PerspTilt(
            &ProjectionMatrix,
            Nintendo3DsPerspectiveFieldOfViewRadians,
            C3D_AspectRatioTop,
            cameraState.NearPlaneDistance,
            cameraState.FarPlaneDistance,
            false);

        ActiveViewMatrix = cameraState.ViewMatrix;
        ActiveCameraOrientation = cameraState.Orientation;
        for (IDrawable3D* drawable : cameraState.Drawables) {
            Visit(drawable);
        }
    }

    /// Resolves one packaged content-relative asset path against the absolute cooked material path that referenced it.
    std::string Nintendo3DsRenderManager3D::ResolvePackagedContentAssetPath(const std::string& cookedMaterialAssetPath, const std::string& contentRelativePath) const {
        if (cookedMaterialAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked material path is required.");
        } else if (contentRelativePath.empty()) {
            throw std::invalid_argument("Nintendo 3DS content-relative asset path is required.");
        }

        std::string normalizedMaterialAssetPath = cookedMaterialAssetPath;
        std::replace(normalizedMaterialAssetPath.begin(), normalizedMaterialAssetPath.end(), '\\', '/');
        const std::size_t cookedMarkerIndex = normalizedMaterialAssetPath.find("/cooked/");
        if (cookedMarkerIndex == std::string::npos) {
            throw std::invalid_argument("Nintendo 3DS cooked material path must contain the packaged '/cooked/' root segment.");
        }

        std::string normalizedContentRelativePath = contentRelativePath;
        std::replace(normalizedContentRelativePath.begin(), normalizedContentRelativePath.end(), '\\', '/');
        const std::string contentRootPath = normalizedMaterialAssetPath.substr(0, cookedMarkerIndex);
        if (!normalizedContentRelativePath.empty() && normalizedContentRelativePath[0] == '/') {
            return contentRootPath + normalizedContentRelativePath;
        }

        return contentRootPath + "/" + normalizedContentRelativePath;
    }

    /// Loads and attaches one cooked diffuse texture when the cooked-material contract references one.
    void Nintendo3DsRenderManager3D::AttachCookedDiffuseTexture(
        Nintendo3DsRuntimeMaterial* runtimeMaterial,
        PlatformMaterialAsset* materialAsset,
        const std::string& cookedMaterialAssetPath) {
        if (runtimeMaterial == nullptr) {
            throw std::invalid_argument("Nintendo 3DS runtime material is required.");
        } else if (materialAsset == nullptr) {
            throw std::invalid_argument("Nintendo 3DS cooked material asset is required.");
        } else if (cookedMaterialAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked material path is required.");
        }

        if (materialAsset->TextureRelativePath.empty()) {
            return;
        }

        const std::string cookedTextureAssetPath = ResolvePackagedContentAssetPath(cookedMaterialAssetPath, materialAsset->TextureRelativePath);
        ::FileStream* textureStream = nullptr;
        ::Asset* textureAssetPayload = nullptr;
        ::TextureAsset* cookedTextureAsset = nullptr;
        Nintendo3DsRuntimeTexture* runtimeTexture = nullptr;
        try {
            textureStream = ::File::OpenRead(cookedTextureAssetPath);
            textureAssetPayload = ::AssetSerializer::Deserialize(textureStream);
            delete textureStream;
            textureStream = nullptr;

            cookedTextureAsset = he_cpp_try_cast<TextureAsset>(textureAssetPayload);
            if (cookedTextureAsset == nullptr) {
                throw std::invalid_argument("Nintendo 3DS cooked diffuse texture payloads must deserialize as TextureAsset.");
            }

            textureAssetPayload = nullptr;
            runtimeTexture = new Nintendo3DsRuntimeTexture();
            runtimeTexture->set_Id(cookedTextureAsset->get_Id());
            runtimeTexture->LoadFromRaw(cookedTextureAsset);
            runtimeMaterial->SetOwnedDiffuseTexture(runtimeTexture);
            runtimeTexture = nullptr;
            delete cookedTextureAsset;
        } catch (...) {
            if (textureStream != nullptr) {
                delete textureStream;
            }
            if (runtimeTexture != nullptr) {
                delete runtimeTexture;
            }
            if (cookedTextureAsset != nullptr) {
                delete cookedTextureAsset;
            }
            if (textureAssetPayload != nullptr) {
                delete textureAssetPayload;
            }

            throw;
        }
    }

    /// Draws one mesh component with one concrete Nintendo 3DS runtime model.
    void Nintendo3DsRenderManager3D::DrawRuntimeModel(MeshComponent* meshComponent, Nintendo3DsRuntimeModel* runtimeModel) {
        if (meshComponent == nullptr || runtimeModel == nullptr || meshComponent->get_Parent() == nullptr) {
            AppendDetailedRenderTrace("Render3D.DrawRuntimeModel: invalid-input");
            return;
        } else if (runtimeModel->GetVertexData() == nullptr || runtimeModel->GetVertexCount() <= 0) {
            AppendDetailedRenderTrace("Render3D.DrawRuntimeModel: missing-vertex-data");
            return;
        }

        Array<RuntimeSubmesh*>* submeshes = runtimeModel->get_Submeshes();
        if (submeshes == nullptr || submeshes->Length == 0) {
            AppendDetailedRenderTrace("Render3D.DrawRuntimeModel: submeshes-empty");
            return;
        }

        if (Nintendo3DsRender3DTraceFramesRemaining > 0) {
            char message[160];
            std::snprintf(
                message,
                sizeof(message),
                "Render3D.DrawRuntimeModel: submeshCount=%d vertexCount=%d",
                static_cast<int>(submeshes->Length),
                static_cast<int>(runtimeModel->GetVertexCount()));
            AppendDetailedRenderTrace(message);
        }

        if (Nintendo3DsRender3DTransformLinesRemaining > 0) {
            Entity* entity = meshComponent->get_Parent();
            float3 position = entity->get_Position();
            float3 scale = entity->get_Scale();
            float4 orientation = entity->get_Orientation();
            char message[256];
            std::snprintf(
                message,
                sizeof(message),
                "Render3D.Transform: entityPos=(%.3f,%.3f,%.3f) scale=(%.3f,%.3f,%.3f) rot=(%.3f,%.3f,%.3f,%.3f)",
                position.X,
                position.Y,
                position.Z,
                scale.X,
                scale.Y,
                scale.Z,
                orientation.X,
                orientation.Y,
                orientation.Z,
                orientation.W);
            AppendTransformRenderTrace(message);
        }

        ::float4x4 world = BuildWorldTransform(meshComponent->get_Parent());
        ::float4x4 modelView;
        float4x4::Multiply__ref0_ref1_out2(world, ActiveViewMatrix, modelView);
        if (Nintendo3DsRender3DTransformLinesRemaining > 0) {
            char message[256];
            std::snprintf(
                message,
                sizeof(message),
                "Render3D.ModelViewOrigin: world=(%.3f,%.3f,%.3f) modelView=(%.3f,%.3f,%.3f,%.3f)",
                world.M41,
                world.M42,
                world.M43,
                modelView.M41,
                modelView.M42,
                modelView.M43,
                modelView.M44);
            AppendTransformRenderTrace(message);
        }

        Nintendo3DsModelVertex* vertexData = runtimeModel->GetVertexData();
        int32_t submittedVertexCount = runtimeModel->GetVertexCount();
        C3D_Mtx gpuModelView = BuildGpuMatrix(modelView);
        GSPGPU_FlushDataCache(vertexData, sizeof(Nintendo3DsModelVertex) * static_cast<uint32_t>(submittedVertexCount));

        DirectionalLightComponent* directionalLight = ResolveActiveDirectionalLight();
        float3 viewSpaceLightVector = BuildViewSpaceLightVector(ActiveCameraOrientation, directionalLight);
        float4 ambientLightColor = ResolveAmbientLightColor();

        for (int32_t submeshIndex = 0; submeshIndex < submeshes->Length; submeshIndex++) {
            RuntimeSubmesh* submesh = (*submeshes)[submeshIndex];
            if (submesh == nullptr || submesh->get_IndexCount() <= 0) {
                continue;
            }

            Nintendo3DsRuntimeMaterial* runtimeMaterial = ResolveRuntimeMaterial(meshComponent, submeshIndex);
            const RuntimeMaterialLightingModel lightingModel = runtimeMaterial == nullptr
                ? RuntimeMaterialLightingModel::MetalRoughPbr
                : runtimeMaterial->get_LightingModel();
            const float4 baseColor = runtimeMaterial == nullptr
                ? float4(1.0f, 1.0f, 1.0f, 1.0f)
                : runtimeMaterial->GetBaseColor();
            float4 directionalLightColor(0.0f, 0.0f, 0.0f, 1.0f);
            float4 effectiveAmbientColor = ambientLightColor;
            if (lightingModel == RuntimeMaterialLightingModel::Unlit) {
                effectiveAmbientColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
            } else if (directionalLight != nullptr) {
                float4 authoredLightColor = directionalLight->get_Color();
                const float intensity = directionalLight->get_Intensity();
                directionalLightColor = float4(
                    authoredLightColor.X * intensity,
                    authoredLightColor.Y * intensity,
                    authoredLightColor.Z * intensity,
                    1.0f);
            }

            if (Nintendo3DsRender3DTransformLinesRemaining > 0) {
                char message[256];
                std::snprintf(
                    message,
                    sizeof(message),
                    "Render3D.Material: lighting=%d base=(%.3f,%.3f,%.3f,%.3f) dir=(%.3f,%.3f,%.3f,%.3f) ambient=(%.3f,%.3f,%.3f,%.3f)",
                    static_cast<int>(lightingModel),
                    baseColor.X,
                    baseColor.Y,
                    baseColor.Z,
                    baseColor.W,
                    directionalLightColor.X,
                    directionalLightColor.Y,
                    directionalLightColor.Z,
                    directionalLightColor.W,
                    effectiveAmbientColor.X,
                    effectiveAmbientColor.Y,
                    effectiveAmbientColor.Z,
                    effectiveAmbientColor.W);
                AppendTransformRenderTrace(message);
            }

            Nintendo3DsRuntimeTexture* diffuseTexture = runtimeMaterial == nullptr
                ? nullptr
                : runtimeMaterial->GetOwnedDiffuseTexture();
            C3D_BufInfo* bufInfo = C3D_GetBufInfo();
            BufInfo_Init(bufInfo);
            if (diffuseTexture != nullptr && diffuseTexture->HasNativeTexture()) {
                EnsureTexturedShaderInitialized();
                C3D_BindProgram(&TexturedProgram);
                ApplyTexturedPipelineState();
                BufInfo_Add(bufInfo, vertexData, sizeof(Nintendo3DsModelVertex), 3, 0x210);
                C3D_TexBind(0, diffuseTexture->GetNativeTexture());
                ApplyCommonLightingUniforms(
                    TexturedUniformLocationProjection,
                    TexturedUniformLocationModelView,
                    TexturedUniformLocationLightVector,
                    TexturedUniformLocationLightColor,
                    TexturedUniformLocationAmbientColor,
                    TexturedUniformLocationBaseColor,
                    ProjectionMatrix,
                    gpuModelView,
                    viewSpaceLightVector,
                    directionalLightColor,
                    effectiveAmbientColor,
                    baseColor);
            } else {
                EnsureShaderInitialized();
                C3D_BindProgram(&Program);
                ApplyUntexturedPipelineState();
                BufInfo_Add(bufInfo, vertexData, sizeof(Nintendo3DsModelVertex), 1, 0x0);
                BufInfo_Add(bufInfo, reinterpret_cast<const uint8_t*>(vertexData) + (sizeof(float) * 5), sizeof(Nintendo3DsModelVertex), 1, 0x1);
                ApplyCommonLightingUniforms(
                    UniformLocationProjection,
                    UniformLocationModelView,
                    UniformLocationLightVector,
                    UniformLocationLightColor,
                    UniformLocationAmbientColor,
                    UniformLocationBaseColor,
                    ProjectionMatrix,
                    gpuModelView,
                    viewSpaceLightVector,
                    directionalLightColor,
                    effectiveAmbientColor,
                    baseColor);
            }

            C3D_DrawArrays(GPU_TRIANGLES, submesh->get_IndexStart(), submesh->get_IndexCount());
        }
    }

    /// Returns whether the supplied camera targets the Nintendo 3DS top screen.
    bool Nintendo3DsRenderManager3D::IsTopScreenCamera(ICamera* camera) const {
        if (camera == nullptr) {
            return false;
        }

        RenderTarget* renderTarget = camera->get_RenderTarget();
        if (renderTarget == nullptr) {
            return false;
        }

        return renderTarget->get_Width() == Nintendo3DsTopScreenWidth
            && renderTarget->get_Height() == Nintendo3DsTopScreenHeight;
    }

    /// Resolves the current camera near-plane distance using conservative Nintendo 3DS-safe clamps.
    float Nintendo3DsRenderManager3D::ResolveNearPlaneDistance(ICamera* camera) const {
        if (camera == nullptr) {
            throw std::invalid_argument("Nintendo 3DS near-plane resolution requires one camera.");
        }

        const float farPlaneDistance = camera->get_FarPlaneDistance();
        const float minimumFarPlaneDistance = farPlaneDistance > Nintendo3DsMinimumNearPlaneDistance + Nintendo3DsMinimumPlaneSeparation
            ? farPlaneDistance
            : Nintendo3DsMinimumNearPlaneDistance + Nintendo3DsMinimumPlaneSeparation;
        const float nearPlaneDistance = camera->get_NearPlaneDistance();
        if (nearPlaneDistance < Nintendo3DsMinimumNearPlaneDistance) {
            return Nintendo3DsMinimumNearPlaneDistance;
        }

        const float maximumNearPlaneDistance = minimumFarPlaneDistance - Nintendo3DsMinimumPlaneSeparation;
        return nearPlaneDistance > maximumNearPlaneDistance ? maximumNearPlaneDistance : nearPlaneDistance;
    }

    /// Resolves the current camera far-plane distance using conservative Nintendo 3DS-safe clamps.
    float Nintendo3DsRenderManager3D::ResolveFarPlaneDistance(ICamera* camera, float nearPlaneDistance) const {
        if (camera == nullptr) {
            throw std::invalid_argument("Nintendo 3DS far-plane resolution requires one camera.");
        }

        const float minimumFarPlaneDistance = nearPlaneDistance + Nintendo3DsMinimumPlaneSeparation;
        const float farPlaneDistance = camera->get_FarPlaneDistance();
        return farPlaneDistance < minimumFarPlaneDistance ? minimumFarPlaneDistance : farPlaneDistance;
    }

    /// Builds the current camera view matrix from its parent entity transform.
    float4x4 Nintendo3DsRenderManager3D::BuildCameraViewMatrix(ICamera* camera) const {
        if (camera == nullptr || camera->get_Parent() == nullptr) {
            throw std::invalid_argument("Nintendo 3DS view-matrix generation requires one camera with one parent entity.");
        }

        ::float3 cameraPosition = camera->get_Parent()->get_Position();
        ::float4 cameraOrientation = camera->get_Parent()->get_Orientation();
        ::float3 cameraForward = float4::RotateVector(::float3(0.0f, 0.0f, -1.0f), cameraOrientation);
        ::float3 cameraUp = float4::RotateVector(::float3(0.0f, 1.0f, 0.0f), cameraOrientation);
        ::float3 cameraTarget = cameraPosition + cameraForward;

        ::float4x4 view;
        float4x4::CreateLookAt__ref0_ref1_ref2_out3(cameraPosition, cameraTarget, cameraUp, view);
        return view;
    }

    /// Builds the current drawable world transform from entity position, orientation, and scale.
    float4x4 Nintendo3DsRenderManager3D::BuildWorldTransform(Entity* entity) const {
        if (entity == nullptr) {
            throw std::invalid_argument("Nintendo 3DS world-transform generation requires one entity.");
        }

        ::float4 orientation = entity->get_Orientation();
        ::float4x4 rotation;
        float4x4::CreateFromQuaternion__ref0_out1(orientation, rotation);

        ::float3 scale = entity->get_Scale();
        ::float4x4 size;
        float4x4::CreateScale__out3(scale.X, scale.Y, scale.Z, size);

        ::float4x4 rotationScale;
        float4x4::Multiply__ref0_ref1_out2(rotation, size, rotationScale);

        ::float3 position = entity->get_Position();
        ::float4x4 translation;
        float4x4::CreateTranslation__ref0_out1(position, translation);

        ::float4x4 world;
        float4x4::Multiply__ref0_ref1_out2(rotationScale, translation, world);
        return world;
    }

    /// Converts one HelEngine row-major float4x4 into the transposed citro3d row-vector layout used by `C3D_FVUnifMtx4x4`.
    C3D_Mtx Nintendo3DsRenderManager3D::BuildGpuMatrix(const float4x4& matrix) const {
        C3D_Mtx gpuModelView;
        gpuModelView.r[0].x = matrix.M11;
        gpuModelView.r[0].y = matrix.M21;
        gpuModelView.r[0].z = matrix.M31;
        gpuModelView.r[0].w = matrix.M41;
        gpuModelView.r[1].x = matrix.M12;
        gpuModelView.r[1].y = matrix.M22;
        gpuModelView.r[1].z = matrix.M32;
        gpuModelView.r[1].w = matrix.M42;
        gpuModelView.r[2].x = matrix.M13;
        gpuModelView.r[2].y = matrix.M23;
        gpuModelView.r[2].z = matrix.M33;
        gpuModelView.r[2].w = matrix.M43;
        gpuModelView.r[3].x = matrix.M14;
        gpuModelView.r[3].y = matrix.M24;
        gpuModelView.r[3].z = matrix.M34;
        gpuModelView.r[3].w = matrix.M44;
        return gpuModelView;
    }

    /// Resolves one Nintendo 3DS runtime material for the active submesh slot when authored materials are available.
    Nintendo3DsRuntimeMaterial* Nintendo3DsRenderManager3D::ResolveRuntimeMaterial(MeshComponent* meshComponent, int32_t submeshIndex) const {
        if (meshComponent == nullptr) {
            return nullptr;
        }

        Array<RuntimeMaterial*>* materials = meshComponent->get_Materials();
        if (materials == nullptr || materials->Length == 0) {
            return nullptr;
        }

        const int32_t materialIndex = submeshIndex < materials->Length ? submeshIndex : 0;
        return dynamic_cast<Nintendo3DsRuntimeMaterial*>((*materials)[materialIndex]);
    }

    /// Resolves the first active directional light registered with the generated-core object manager.
    DirectionalLightComponent* Nintendo3DsRenderManager3D::ResolveActiveDirectionalLight() const {
        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            return nullptr;
        }

        List<DirectionalLightComponent*>* directionalLights = core->get_ObjectManager()->get_DirectionalLights();
        if (directionalLights == nullptr) {
            return nullptr;
        }

        for (int32_t lightIndex = 0; lightIndex < directionalLights->get_Count(); lightIndex++) {
            DirectionalLightComponent* directionalLight = (*directionalLights)[lightIndex];
            if (directionalLight == nullptr || directionalLight->get_Parent() == nullptr || !directionalLight->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            return directionalLight;
        }

        return nullptr;
    }

    /// Resolves the accumulated active ambient-light color registered with the generated-core object manager.
    float4 Nintendo3DsRenderManager3D::ResolveAmbientLightColor() const {
        float4 ambientLightColor(0.0f, 0.0f, 0.0f, 1.0f);
        Core* core = Core::get_Instance();
        if (core == nullptr || core->get_ObjectManager() == nullptr) {
            return ambientLightColor;
        }

        List<AmbientLightComponent*>* ambientLights = core->get_ObjectManager()->get_AmbientLights();
        if (ambientLights == nullptr) {
            return ambientLightColor;
        }

        for (int32_t lightIndex = 0; lightIndex < ambientLights->get_Count(); lightIndex++) {
            AmbientLightComponent* ambientLight = (*ambientLights)[lightIndex];
            if (ambientLight == nullptr || ambientLight->get_Parent() == nullptr || !ambientLight->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }

            float4 authoredAmbientColor = ambientLight->get_Color();
            const float intensity = ambientLight->get_Intensity();
            ambientLightColor.X += authoredAmbientColor.X * intensity;
            ambientLightColor.Y += authoredAmbientColor.Y * intensity;
            ambientLightColor.Z += authoredAmbientColor.Z * intensity;
        }

        return ambientLightColor;
    }

    /// Builds the active directional-light vector in view space so the Nintendo 3DS shader can evaluate Lambert lighting.
    float3 Nintendo3DsRenderManager3D::BuildViewSpaceLightVector(const float4& cameraOrientation, DirectionalLightComponent* directionalLight) const {
        if (directionalLight == nullptr || directionalLight->get_Parent() == nullptr) {
            return float3(0.0f, 0.0f, -1.0f);
        }

        float3 worldSpaceLightVector = float4::RotateVector(float3(0.0f, 0.0f, -1.0f), directionalLight->get_Parent()->get_Orientation());
        worldSpaceLightVector = float3::Normalize(worldSpaceLightVector);
        float4 inverseCameraOrientation = float4::Inverse(cameraOrientation);
        float3 viewSpaceLightVector = float4::RotateVector(worldSpaceLightVector, inverseCameraOrientation);
        return float3::Normalize(viewSpaceLightVector);
    }
}

#endif
