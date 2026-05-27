#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include "RenderManager3D.hpp"

namespace helengine::nintendo3ds {
    /// Materializes runtime 3D assets for startup-scene loading without issuing any draw calls.
    class Nintendo3DsStartupRenderManager3D : public RenderManager3D {
    public:
        /// Builds one lightweight runtime model from the supplied raw asset.
        /// <param name="data">Raw model asset requested by the runtime scene loader.</param>
        /// <returns>Lightweight runtime model populated with bounds and runtime submeshes.</returns>
        RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;

        /// Builds one lightweight runtime model for cooked model requests during startup-scene materialization.
        /// <param name="cookedAssetPath">Cooked model asset path requested by the runtime scene loader.</param>
        /// <returns>Lightweight runtime model placeholder.</returns>
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
    };
}

#endif
