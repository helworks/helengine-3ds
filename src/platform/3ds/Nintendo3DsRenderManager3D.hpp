#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <cstdint>
#include <string>
#include <vector>

#include <citro3d.h>

#include "IDrawable3D.hpp"
#include "IRenderVisitor3D.hpp"
#include "RenderManager3D.hpp"
#include "float4x4.hpp"

class ContentManager;
class ICamera;
class ModelAsset;
class PlatformMaterialAsset;
class RuntimeMaterial;
class RuntimeModel;
class MeshComponent;
class Entity;
class RuntimeSubmesh;

namespace helengine::nintendo3ds {
    /// Captures generated-core 3D cameras and renders solid-color meshes through one native citro3d top-screen pass.
    class Nintendo3DsRenderManager3D final : public RenderManager3D, public IRenderVisitor3D {
    public:
        /// Creates the Nintendo 3DS 3D renderer for startup-scene materialization and queued 3D playback.
        Nintendo3DsRenderManager3D();

        /// Releases shader resources owned by the Nintendo 3DS 3D renderer.
        ~Nintendo3DsRenderManager3D() override;

        /// Builds one Nintendo 3DS runtime model from the supplied raw asset.
        /// <param name="data">Raw model asset requested by the runtime scene loader.</param>
        /// <returns>Nintendo 3DS runtime model populated with bounds and runtime submeshes.</returns>
        RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;

        /// Builds one Nintendo 3DS runtime model for cooked model requests during startup-scene materialization.
        /// <param name="cookedAssetPath">Cooked model asset path requested by the runtime scene loader.</param>
        /// <returns>Nintendo 3DS runtime model placeholder.</returns>
        RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath) override;

        /// Builds one lightweight runtime material from one platform-owned cooked material asset.
        /// <param name="materialAsset">Cooked material asset requested by the runtime scene loader.</param>
        /// <returns>Lightweight runtime material placeholder.</returns>
        RuntimeMaterial* BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) override;

        /// Builds one lightweight runtime material from one cooked material asset path.
        /// <param name="cookedAssetPath">Cooked material asset path requested by the runtime scene loader.</param>
        /// <returns>Lightweight runtime material placeholder.</returns>
        RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath) override;

        /// Builds one lightweight runtime material from one raw material asset path.
        /// <param name="assetContentManager">Content manager that can deserialize companion shader packages.</param>
        /// <param name="contentRootPath">Absolute packaged content root.</param>
        /// <param name="materialAssetPath">Absolute path to the serialized material asset.</param>
        /// <returns>Lightweight runtime material placeholder.</returns>
        RuntimeMaterial* BuildMaterialFromRawAsset(
            ContentManager* assetContentManager,
            std::string contentRootPath,
            std::string materialAssetPath) override;

        /// Releases one Nintendo 3DS runtime model after the final scene reference is removed.
        /// <param name="model">Runtime model being released by the generated core.</param>
        void ReleaseModel(RuntimeModel* model) override;

        /// Walks the active generated-core camera list and captures the top-screen cameras that should render during present-time playback.
        void Draw() override;

        /// Visits one ordered 3D drawable from the active generated-core camera queue and draws supported mesh content through the solid-color path.
        /// <param name="drawable">Ordered drawable visited from the active camera queue.</param>
        void Visit(IDrawable3D* drawable) override;

        /// Clears the previous frame's captured 3D camera list before the next generated-core draw begins.
        void BeginFrame();

        /// Replays the captured 3D frame state onto the supplied Nintendo 3DS render target.
        /// <param name="target">Top-screen render target that will own the citro3d playback pass.</param>
        /// <param name="clearColor">Top-screen clear color that should be applied through citro3d before playback begins.</param>
        void RenderTopScreen(C3D_RenderTarget* target, u32 clearColor);

    private:
        /// Stores the top-screen cameras captured for the active frame in generated-core draw order.
        std::vector<ICamera*> CapturedCameras;

        /// Stores the shared perspective projection uploaded before one camera draw.
        C3D_Mtx ProjectionMatrix;

        /// Stores the camera whose 3D queue is currently being traversed.
        ICamera* ActiveCamera;

        /// Stores the active view matrix used while drawing one captured camera.
        float4x4 ActiveViewMatrix;

        /// Stores the active target currently receiving 3D output during present-time playback.
        C3D_RenderTarget* ActiveTarget;

        /// Stores the parsed vertex-shader binary used by the solid-color mesh path.
        DVLB_s* VertexShaderDvlb;

        /// Stores the bound shader program used by the solid-color mesh path.
        shaderProgram_s Program;

        /// Stores whether the shader program has been initialized.
        bool HasShaderProgram;

        /// Stores the bound projection uniform location.
        int UniformLocationProjection;

        /// Stores the bound model-view uniform location.
        int UniformLocationModelView;

        /// Stores the first active top-screen clear color captured from the generated-core camera list.
        u32 TopScreenClearColor;

        /// Stores whether the current frame captured one top-screen clear color.
        bool HasTopScreenClearColor;

        /// Initializes the solid-color shader program, attribute layout, and fixed fragment state the first time one 3D frame is rendered.
        void EnsureShaderInitialized();

        /// Releases the shader program and parsed shader binary when the renderer is destroyed.
        void ReleaseShaderResources();

        /// Reapplies the fixed Citro3D pipeline state required by the solid-color 3D path after Citro2D work may have changed GPU state.
        void ApplyPipelineState();

        /// Draws one captured camera through the solid-color top-screen path.
        /// <param name="camera">Top-screen camera captured during the active frame.</param>
        void DrawCamera(ICamera* camera);

        /// Draws one mesh component with one concrete Nintendo 3DS runtime model.
        /// <param name="meshComponent">Mesh component currently being visited from the active camera queue.</param>
        /// <param name="runtimeModel">Nintendo 3DS runtime model bound to the mesh component.</param>
        void DrawRuntimeModel(MeshComponent* meshComponent, class Nintendo3DsRuntimeModel* runtimeModel);

        /// Returns whether the supplied camera targets the Nintendo 3DS top screen.
        /// <param name="camera">Camera being classified for present-time playback.</param>
        /// <returns>`true` when the camera resolves to the top screen; otherwise `false`.</returns>
        bool IsTopScreenCamera(ICamera* camera) const;

        /// Resolves the current camera near-plane distance using conservative Nintendo 3DS-safe clamps.
        /// <param name="camera">Camera whose near plane should be normalized.</param>
        /// <returns>Clamped near-plane distance.</returns>
        float ResolveNearPlaneDistance(ICamera* camera) const;

        /// Resolves the current camera far-plane distance using conservative Nintendo 3DS-safe clamps.
        /// <param name="camera">Camera whose far plane should be normalized.</param>
        /// <param name="nearPlaneDistance">Already-clamped near-plane distance.</param>
        /// <returns>Clamped far-plane distance.</returns>
        float ResolveFarPlaneDistance(ICamera* camera, float nearPlaneDistance) const;

        /// Builds the current camera view matrix from its parent entity transform.
        /// <param name="camera">Camera being prepared for the active draw pass.</param>
        /// <returns>View matrix used by the Nintendo 3DS shader path.</returns>
        float4x4 BuildCameraViewMatrix(ICamera* camera) const;

        /// Builds the current drawable world transform from entity position, orientation, and scale.
        /// <param name="entity">Entity owning the active mesh component.</param>
        /// <returns>World transform matrix for the drawable.</returns>
        float4x4 BuildWorldTransform(Entity* entity) const;

        /// Converts one HelEngine row-major float4x4 into the row-major citro3d matrix layout used by `C3D_FVUnifMtx4x4`.
        /// <param name="matrix">HelEngine matrix being copied into GPU uniform layout.</param>
        /// <returns>Citro3d matrix with matching row-major values.</returns>
        C3D_Mtx BuildGpuMatrix(const float4x4& matrix) const;
    };
}

#endif
