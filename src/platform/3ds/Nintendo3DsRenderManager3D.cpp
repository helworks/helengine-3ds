#include "platform/3ds/Nintendo3DsRenderManager3D.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <3ds.h>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "Core.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "IRenderQueue3D.hpp"
#include "MeshComponent.hpp"
#include "ModelAsset.hpp"
#include "ModelAssetIndexData.hpp"
#include "ModelSubmeshResolver.hpp"
#include "ObjectManager.hpp"
#include "RenderTarget.hpp"
#include "RuntimeMaterial.hpp"
#include "RuntimeSubmesh.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "runtime/native_cast.hpp"
#include "platform/3ds/Nintendo3DsRuntimeModel.hpp"
#include "solid_color_shbin.h"
#include "system/io/file.hpp"

namespace helengine::nintendo3ds {
    namespace {
        /// Stores the Nintendo 3DS top-screen width in pixels.
        constexpr int32_t Nintendo3DsTopScreenWidth = 400;

        /// Stores the Nintendo 3DS top-screen height in pixels.
        constexpr int32_t Nintendo3DsTopScreenHeight = 240;

        /// Stores the minimum safe near-plane distance for the Nintendo 3DS solid-color path.
        constexpr float Nintendo3DsMinimumNearPlaneDistance = 0.01f;

        /// Stores the minimum safe separation between near and far planes for the Nintendo 3DS solid-color path.
        constexpr float Nintendo3DsMinimumPlaneSeparation = 0.01f;

        /// Stores the fixed field-of-view used by the first Nintendo 3DS solid-color renderer milestone.
        constexpr float Nintendo3DsPerspectiveFieldOfViewRadians = 0.78539816339744831f;

    }

    /// Creates the Nintendo 3DS 3D renderer for startup-scene materialization and queued 3D playback.
    Nintendo3DsRenderManager3D::Nintendo3DsRenderManager3D()
        : CapturedCameras()
        , ProjectionMatrix()
        , ActiveCamera(nullptr)
        , ActiveViewMatrix(float4x4::get_Identity())
        , ActiveTarget(nullptr)
        , VertexShaderDvlb(nullptr)
        , Program()
        , HasShaderProgram(false)
        , UniformLocationProjection(-1)
        , UniformLocationModelView(-1)
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
        }

        ::ModelAssetIndexData* indexData = ::ModelAssetIndexData::Resolve(data);
        std::vector<::float3> expandedPositions;
        try {
            const bool hasIndices = indexData != nullptr && indexData->IndexCount > 0;
            expandedPositions.reserve(static_cast<std::size_t>(hasIndices ? indexData->IndexCount : data->Positions->Length));
            if (hasIndices && indexData->Uses32BitIndices && indexData->Indices32 != nullptr) {
                for (int32_t index = 0; index < indexData->Indices32->Length; index++) {
                    const uint32_t sourceIndex = (*indexData->Indices32)[index];
                    if (sourceIndex >= static_cast<uint32_t>(data->Positions->Length)) {
                        throw std::out_of_range("Nintendo 3DS model index exceeds the authored position range.");
                    }

                    expandedPositions.push_back((*data->Positions)[sourceIndex]);
                }
            } else if (hasIndices && indexData->Indices16 != nullptr) {
                for (int32_t index = 0; index < indexData->Indices16->Length; index++) {
                    const uint16_t sourceIndex = (*indexData->Indices16)[index];
                    if (sourceIndex >= static_cast<uint16_t>(data->Positions->Length)) {
                        throw std::out_of_range("Nintendo 3DS model index exceeds the authored position range.");
                    }

                    expandedPositions.push_back((*data->Positions)[sourceIndex]);
                }
            } else {
                for (int32_t index = 0; index < data->Positions->Length; index++) {
                    expandedPositions.push_back((*data->Positions)[index]);
                }
            }
        } catch (...) {
            delete indexData;
            throw;
        }

        delete indexData;

        float3* expandedVertexData = static_cast<float3*>(linearAlloc(sizeof(float3) * expandedPositions.size()));
        if (expandedVertexData == nullptr) {
            throw std::bad_alloc();
        }

        std::memcpy(expandedVertexData, expandedPositions.data(), sizeof(float3) * expandedPositions.size());
        GSPGPU_FlushDataCache(expandedVertexData, sizeof(float3) * expandedPositions.size());
        Nintendo3DsRuntimeModel* runtimeModel = new Nintendo3DsRuntimeModel(expandedVertexData, static_cast<int32_t>(expandedPositions.size()));
        runtimeModel->SetBounds(data->BoundsMin, data->BoundsMax);
        runtimeModel->SetSubmeshes(ModelSubmeshResolver::BuildRuntimeSubmeshes(data));
        return runtimeModel;
    }

    /// Builds one Nintendo 3DS runtime model for cooked model requests during startup-scene materialization.
    RuntimeModel* Nintendo3DsRenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
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

        return new RuntimeMaterial();
    }

    /// Builds one lightweight runtime material from one cooked material asset path.
    RuntimeMaterial* Nintendo3DsRenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked material asset path is required.");
        }

        return new RuntimeMaterial();
    }

    /// Builds one lightweight runtime material from one raw material asset path.
    RuntimeMaterial* Nintendo3DsRenderManager3D::BuildMaterialFromRawAsset(
        ContentManager* assetContentManager,
        std::string contentRootPath,
        std::string materialAssetPath) {
        if (assetContentManager == nullptr) {
            throw std::invalid_argument("Nintendo 3DS material content manager is required.");
        } else if (contentRootPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS material content root path is required.");
        } else if (materialAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS material asset path is required.");
        }

        return new RuntimeMaterial();
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
            return;
        }

        List<ICamera*>* cameras = core->get_ObjectManager()->get_Cameras();
        if (cameras == nullptr) {
            return;
        }

        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            ICamera* camera = (*cameras)[cameraIndex];
            if (camera == nullptr || camera->get_Parent() == nullptr || !camera->get_Parent()->get_IsHierarchyEnabled()) {
                continue;
            }
            if (!IsTopScreenCamera(camera)) {
                continue;
            }

            CapturedCameras.push_back(camera);
        }

    }

    /// Visits one ordered 3D drawable from the active generated-core camera queue and draws supported mesh content through the solid-color path.
    void Nintendo3DsRenderManager3D::Visit(IDrawable3D* drawable) {
        if (drawable == nullptr || ActiveCamera == nullptr) {
            return;
        } else if (drawable->get_Parent() == nullptr || !drawable->get_Parent()->get_IsHierarchyEnabled()) {
            return;
        }

        MeshComponent* meshComponent = he_cpp_try_cast<MeshComponent>(drawable);
        if (meshComponent == nullptr || meshComponent->get_Model() == nullptr) {
            return;
        }

        Nintendo3DsRuntimeModel* runtimeModel = dynamic_cast<Nintendo3DsRuntimeModel*>(meshComponent->get_Model());
        if (runtimeModel == nullptr) {
            return;
        }

        DrawRuntimeModel(meshComponent, runtimeModel);
    }

    /// Clears the previous frame's captured 3D camera list before the next generated-core draw begins.
    void Nintendo3DsRenderManager3D::BeginFrame() {
        CapturedCameras.clear();
        ActiveCamera = nullptr;
        ActiveViewMatrix = float4x4::get_Identity();
        ActiveTarget = nullptr;
        HasTopScreenClearColor = false;
        TopScreenClearColor = 0;
    }

    /// Replays the captured 3D frame state onto the supplied Nintendo 3DS render target.
    void Nintendo3DsRenderManager3D::RenderTopScreen(C3D_RenderTarget* target, u32 clearColor) {
        if (target == nullptr) {
            throw std::invalid_argument("Nintendo 3DS top-screen render target is required.");
        }

        EnsureShaderInitialized();
        ActiveTarget = target;
        C3D_RenderTargetClear(target, C3D_CLEAR_ALL, clearColor, 0);
        C3D_FrameDrawOn(target);
        if (CapturedCameras.empty()) {
            ActiveTarget = nullptr;
            return;
        }

        C3D_BindProgram(&Program);
        ApplyPipelineState();

        for (ICamera* camera : CapturedCameras) {
            DrawCamera(camera);
        }

        ActiveTarget = nullptr;
        ActiveCamera = nullptr;
    }

    /// Initializes the solid-color shader program, attribute layout, and fixed fragment state the first time one 3D frame is rendered.
    void Nintendo3DsRenderManager3D::EnsureShaderInitialized() {
        if (HasShaderProgram) {
            return;
        }

        VertexShaderDvlb = DVLB_ParseFile((u32*)solid_color_shbin, solid_color_shbin_size);
        if (VertexShaderDvlb == nullptr) {
            throw std::runtime_error("Nintendo 3DS solid-color vertex shader parsing failed.");
        }

        shaderProgramInit(&Program);
        shaderProgramSetVsh(&Program, &VertexShaderDvlb->DVLE[0]);
        C3D_BindProgram(&Program);

        UniformLocationProjection = shaderInstanceGetUniformLocation(Program.vertexShader, "projection");
        UniformLocationModelView = shaderInstanceGetUniformLocation(Program.vertexShader, "modelView");

        HasShaderProgram = true;
    }

    /// Reapplies the fixed Citro3D pipeline state required by the solid-color 3D path after Citro2D work may have changed GPU state.
    void Nintendo3DsRenderManager3D::ApplyPipelineState() {
        C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
        AttrInfo_Init(attrInfo);
        AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
        AttrInfo_AddFixed(attrInfo, 1);
        C3D_FixedAttribSet(1, 1.0f, 1.0f, 1.0f, 1.0f);

        C3D_TexEnv* env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
        C3D_CullFace(GPU_CULL_NONE);
    }

    /// Releases the shader program and parsed shader binary when the renderer is destroyed.
    void Nintendo3DsRenderManager3D::ReleaseShaderResources() {
        if (HasShaderProgram) {
            shaderProgramFree(&Program);
            HasShaderProgram = false;
        }
        if (VertexShaderDvlb != nullptr) {
            DVLB_Free(VertexShaderDvlb);
            VertexShaderDvlb = nullptr;
        }

        UniformLocationProjection = -1;
        UniformLocationModelView = -1;
    }

    /// Draws one captured camera through the solid-color top-screen path.
    void Nintendo3DsRenderManager3D::DrawCamera(ICamera* camera) {
        if (camera == nullptr) {
            throw std::invalid_argument("Nintendo 3DS 3D camera is required.");
        }

        IRenderQueue3D* renderQueue = camera->get_RenderQueue3D();
        if (renderQueue == nullptr) {
            return;
        }

        const float nearPlaneDistance = ResolveNearPlaneDistance(camera);
        const float farPlaneDistance = ResolveFarPlaneDistance(camera, nearPlaneDistance);
        Mtx_PerspTilt(
            &ProjectionMatrix,
            Nintendo3DsPerspectiveFieldOfViewRadians,
            C3D_AspectRatioTop,
            nearPlaneDistance,
            farPlaneDistance,
            false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UniformLocationProjection, &ProjectionMatrix);

        ActiveCamera = camera;
        ActiveViewMatrix = BuildCameraViewMatrix(camera);
        renderQueue->VisitOrdered(this);
        ActiveCamera = nullptr;
    }

    /// Draws one mesh component with one concrete Nintendo 3DS runtime model.
    void Nintendo3DsRenderManager3D::DrawRuntimeModel(MeshComponent* meshComponent, Nintendo3DsRuntimeModel* runtimeModel) {
        if (meshComponent == nullptr || runtimeModel == nullptr || meshComponent->get_Parent() == nullptr) {
            return;
        } else if (runtimeModel->GetVertexData() == nullptr || runtimeModel->GetVertexCount() <= 0) {
            return;
        }

        Array<RuntimeSubmesh*>* submeshes = runtimeModel->get_Submeshes();
        if (submeshes == nullptr || submeshes->Length == 0) {
            return;
        }

        ::float4x4 world = BuildWorldTransform(meshComponent->get_Parent());
        ::float4x4 modelView;
        float4x4::Multiply__ref0_ref1_out2(world, ActiveViewMatrix, modelView);
        float3* vertexData = runtimeModel->GetVertexData();
        int32_t submittedVertexCount = runtimeModel->GetVertexCount();
        C3D_Mtx gpuModelView = BuildGpuMatrix(modelView);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UniformLocationModelView, &gpuModelView);
        GSPGPU_FlushDataCache(vertexData, sizeof(float3) * static_cast<uint32_t>(submittedVertexCount));
        C3D_BufInfo* bufInfo = C3D_GetBufInfo();
        BufInfo_Init(bufInfo);
        BufInfo_Add(bufInfo, vertexData, sizeof(float3), 1, 0x0);

        for (int32_t submeshIndex = 0; submeshIndex < submeshes->Length; submeshIndex++) {
            RuntimeSubmesh* submesh = (*submeshes)[submeshIndex];
            if (submesh == nullptr) {
                continue;
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
}

#endif
