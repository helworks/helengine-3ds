#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <cstdint>
#include <string>
#include <vector>

#include <citro3d.h>

#include "IDrawable3D.hpp"
#include "IRenderVisitor3D.hpp"
#include "RenderManager3D.hpp"
#include "float3.hpp"
#include "float4.hpp"
#include "float4x4.hpp"

class ContentManager;
class ICamera;
class float3;
class float4;
class ModelAsset;
class PlatformMaterialAsset;
class RuntimeMaterial;
class RuntimeModel;
class AmbientLightComponent;
class DirectionalLightComponent;
class MeshComponent;
class Entity;
class RuntimeSubmesh;

namespace helengine::nintendo3ds {
    /// Captures generated-core 3D cameras and renders authored meshes through one native citro3d top-screen pass.
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
        RuntimeModel* BuildModelFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Builds one lightweight runtime material from one platform-owned cooked material asset.
        /// <param name="materialAsset">Cooked material asset requested by the runtime scene loader.</param>
        /// <returns>Lightweight runtime material placeholder.</returns>
        RuntimeMaterial* BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) override;

        /// Builds one lightweight runtime material from one cooked material asset path.
        /// <param name="cookedAssetPath">Cooked material asset path requested by the runtime scene loader.</param>
        /// <returns>Lightweight runtime material placeholder.</returns>
        RuntimeMaterial* BuildMaterialFromCooked(std::string cookedAssetPath, IContentStreamSource* contentStreamSource) override;

        /// Builds one lightweight runtime material from one raw material asset path.
        /// <param name="assetContentManager">Content manager that can deserialize companion shader packages.</param>
        /// <param name="materialAssetPath">Absolute path to the serialized material asset.</param>
        /// <returns>Lightweight runtime material placeholder.</returns>
        RuntimeMaterial* BuildMaterialFromRawAsset(
            ContentManager* assetContentManager,
            std::string materialAssetPath) override;

        /// Releases one Nintendo 3DS runtime model after the final scene reference is removed.
        /// <param name="model">Runtime model being released by the generated core.</param>
        void ReleaseModel(RuntimeModel* model) override;

        /// Walks the active generated-core camera list and captures the top-screen cameras that should render during present-time playback.
        void Draw() override;

        /// Visits one ordered 3D drawable from the active generated-core camera queue and draws supported mesh content through the active lit-material path.
        /// <param name="drawable">Ordered drawable visited from the active camera queue.</param>
        void Visit(IDrawable3D* drawable) override;

        /// Clears the previous frame's captured 3D camera list before the next generated-core draw begins.
        void BeginFrame();

        /// Replays the captured 3D frame state onto the supplied Nintendo 3DS render target.
        /// <param name="target">Top-screen render target that will own the citro3d playback pass.</param>
        /// <param name="clearColor">Top-screen clear color that should be applied through citro3d before playback begins.</param>
        void RenderTopScreen(C3D_RenderTarget* target, u32 clearColor);

    private:
        /// Stores one immutable top-screen camera snapshot captured during generated-core draw submission.
        struct CapturedCameraState {
            /// Stores the ordered 3D drawables visible to the captured camera during generated-core draw submission.
            std::vector<IDrawable3D*> Drawables;

            /// Stores the generated-core camera view matrix captured before any later runtime mutation can occur.
            float4x4 ViewMatrix;

            /// Stores the captured camera world orientation used to rebuild view-space lighting during replay.
            float4 Orientation;

            /// Stores the captured camera world position used by transform diagnostics during replay.
            float3 Position;

            /// Stores the already-clamped near plane captured for replay.
            float NearPlaneDistance;

            /// Stores the already-clamped far plane captured for replay.
            float FarPlaneDistance;

            /// Stores the drawable count reported by the live render queue at capture time.
            int32_t QueueCount;

            /// Stores the queue capacity reported by the live render queue at capture time.
            int32_t QueueCapacity;
        };

        /// Stores the top-screen cameras captured for the active frame in generated-core draw order.
        std::vector<CapturedCameraState> CapturedCameras;

        /// Stores the shared perspective projection uploaded before one camera draw.
        C3D_Mtx ProjectionMatrix;

        /// Stores the active view matrix used while drawing one captured camera.
        float4x4 ActiveViewMatrix;

        /// Stores the active camera orientation used while resolving view-space lighting during replay.
        float4 ActiveCameraOrientation;

        /// Stores the active target currently receiving 3D output during present-time playback.
        C3D_RenderTarget* ActiveTarget;

        /// Stores the capture buffer currently receiving ordered drawables from one live camera queue.
        std::vector<IDrawable3D*>* ActiveCapturedDrawables;

        /// Stores the parsed vertex-shader binary used by the lit untextured mesh path.
        DVLB_s* VertexShaderDvlb;

        /// Stores the bound shader program used by the lit untextured mesh path.
        shaderProgram_s Program;

        /// Stores whether the lit untextured shader program has been initialized.
        bool HasShaderProgram;

        /// Stores the parsed vertex-shader binary used by the lit textured mesh path.
        DVLB_s* TexturedVertexShaderDvlb;

        /// Stores the bound shader program used by the lit textured mesh path.
        shaderProgram_s TexturedProgram;

        /// Stores whether the lit textured shader program has been initialized.
        bool HasTexturedShaderProgram;

        /// Stores the bound projection uniform location.
        int UniformLocationProjection;

        /// Stores the bound model-view uniform location.
        int UniformLocationModelView;

        /// Stores the bound directional-light vector uniform location.
        int UniformLocationLightVector;

        /// Stores the bound directional-light color uniform location.
        int UniformLocationLightColor;

        /// Stores the bound ambient-light color uniform location.
        int UniformLocationAmbientColor;

        /// Stores the bound material base-color uniform location.
        int UniformLocationBaseColor;

        /// Stores the bound projection uniform location for the textured shader path.
        int TexturedUniformLocationProjection;

        /// Stores the bound model-view uniform location for the textured shader path.
        int TexturedUniformLocationModelView;

        /// Stores the bound directional-light vector uniform location for the textured shader path.
        int TexturedUniformLocationLightVector;

        /// Stores the bound directional-light color uniform location for the textured shader path.
        int TexturedUniformLocationLightColor;

        /// Stores the bound ambient-light color uniform location for the textured shader path.
        int TexturedUniformLocationAmbientColor;

        /// Stores the bound material base-color uniform location for the textured shader path.
        int TexturedUniformLocationBaseColor;

        /// Stores the first active top-screen clear color captured from the generated-core camera list.
        u32 TopScreenClearColor;

        /// Stores whether the current frame captured one top-screen clear color.
        bool HasTopScreenClearColor;

        /// Initializes the lit untextured shader program the first time one untextured 3D draw is submitted.
        void EnsureShaderInitialized();

        /// Initializes the lit textured shader program the first time one textured 3D draw is submitted.
        void EnsureTexturedShaderInitialized();

        /// Releases the shader program and parsed shader binary when the renderer is destroyed.
        void ReleaseShaderResources();

        /// Reapplies the fixed Citro3D pipeline state required by the lit untextured 3D path after Citro2D work may have changed GPU state.
        void ApplyUntexturedPipelineState();

        /// Reapplies the fixed Citro3D pipeline state required by the lit textured 3D path after Citro2D work may have changed GPU state.
        void ApplyTexturedPipelineState();

        /// Uploads one shared set of projection, transform, lighting, and base-color uniforms to the currently bound shader program.
        /// <param name="projectionLocation">Projection uniform location on the currently bound program.</param>
        /// <param name="modelViewLocation">Model-view uniform location on the currently bound program.</param>
        /// <param name="lightVectorLocation">Directional-light vector uniform location on the currently bound program.</param>
        /// <param name="lightColorLocation">Directional-light color uniform location on the currently bound program.</param>
        /// <param name="ambientColorLocation">Ambient-light color uniform location on the currently bound program.</param>
        /// <param name="baseColorLocation">Material base-color uniform location on the currently bound program.</param>
        /// <param name="projectionMatrix">Projection matrix for the active camera.</param>
        /// <param name="modelViewMatrix">Model-view matrix for the active drawable.</param>
        /// <param name="viewSpaceLightVector">View-space light direction used by the Lambert lighting path.</param>
        /// <param name="directionalLightColor">Directional-light color resolved for the active draw.</param>
        /// <param name="ambientLightColor">Ambient-light color resolved for the active draw.</param>
        /// <param name="baseColor">Normalized material base color resolved for the active draw.</param>
        void ApplyCommonLightingUniforms(
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
            const float4& baseColor);

        /// Draws one captured camera through the lit-color top-screen path.
        /// <param name="cameraState">Immutable top-screen camera state captured during the active frame.</param>
        void DrawCamera(const CapturedCameraState& cameraState);

        /// Draws one mesh component with one concrete Nintendo 3DS runtime model.
        /// <param name="meshComponent">Mesh component currently being visited from the active camera queue.</param>
        /// <param name="runtimeModel">Nintendo 3DS runtime model bound to the mesh component.</param>
        void DrawRuntimeModel(MeshComponent* meshComponent, class Nintendo3DsRuntimeModel* runtimeModel);

        /// Resolves one packaged content-relative asset path against the absolute cooked material path that referenced it.
        /// <param name="cookedMaterialAssetPath">Absolute cooked material asset path that owns the relative asset reference.</param>
        /// <param name="contentRelativePath">Packaged content-relative asset path stored inside the cooked material payload.</param>
        /// <returns>Resolved packaged content asset path that can be opened by the runtime.</returns>
        std::string ResolvePackagedContentAssetPath(const std::string& cookedMaterialAssetPath, const std::string& contentRelativePath) const;

        /// Loads and attaches one cooked diffuse texture when the cooked-material contract references one.
        /// <param name="runtimeMaterial">Runtime material that will own the rebuilt diffuse texture.</param>
        /// <param name="materialAsset">Cooked material payload that contains the relative diffuse-texture path.</param>
        /// <param name="cookedMaterialAssetPath">Absolute cooked material asset path that referenced the diffuse texture.</param>
        void AttachCookedDiffuseTexture(
            class Nintendo3DsRuntimeMaterial* runtimeMaterial,
            PlatformMaterialAsset* materialAsset,
            const std::string& cookedMaterialAssetPath);

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

        /// Resolves one Nintendo 3DS runtime material for the active submesh slot when authored materials are available.
        /// <param name="meshComponent">Mesh component that owns the authored runtime-material slots.</param>
        /// <param name="submeshIndex">Submesh slot currently being drawn.</param>
        /// <returns>Nintendo 3DS runtime material when one is available; otherwise `nullptr`.</returns>
        class Nintendo3DsRuntimeMaterial* ResolveRuntimeMaterial(MeshComponent* meshComponent, int32_t submeshIndex) const;

        /// Resolves the first active directional light registered with the generated-core object manager.
        /// <returns>First active directional light when one is available; otherwise `nullptr`.</returns>
        DirectionalLightComponent* ResolveActiveDirectionalLight() const;

        /// Resolves the accumulated active ambient-light color registered with the generated-core object manager.
        /// <returns>Accumulated normalized ambient-light color for the active frame.</returns>
        float4 ResolveAmbientLightColor() const;

        /// Builds the active directional-light vector in view space so the Nintendo 3DS shader can evaluate Lambert lighting.
        /// <param name="cameraOrientation">Captured camera world orientation whose inverse rotates the light into view space.</param>
        /// <param name="directionalLight">Directional light contributing to the active draw.</param>
        /// <returns>Normalized view-space light direction.</returns>
        float3 BuildViewSpaceLightVector(const float4& cameraOrientation, DirectionalLightComponent* directionalLight) const;
    };
}

#endif
