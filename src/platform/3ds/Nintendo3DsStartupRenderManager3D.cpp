#include "platform/3ds/Nintendo3DsStartupRenderManager3D.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <stdexcept>

#include "ModelAsset.hpp"
#include "ModelSubmeshResolver.hpp"
#include "platform/3ds/Nintendo3DsStartupRuntimeModel.hpp"
#include "RuntimeMaterial.hpp"

namespace helengine::nintendo3ds {
    /// Builds one lightweight runtime model from the supplied raw asset.
    RuntimeModel* Nintendo3DsStartupRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
        if (data == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup model asset is required.");
        }

        Nintendo3DsStartupRuntimeModel* runtimeModel = new Nintendo3DsStartupRuntimeModel();
        runtimeModel->SetBounds(data->BoundsMin, data->BoundsMax);
        runtimeModel->SetSubmeshes(ModelSubmeshResolver::BuildRuntimeSubmeshes(data));
        return runtimeModel;
    }

    /// Builds one lightweight runtime model for cooked model requests during startup-scene materialization.
    RuntimeModel* Nintendo3DsStartupRenderManager3D::BuildModelFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked model asset path is required.");
        }

        Nintendo3DsStartupRuntimeModel* runtimeModel = new Nintendo3DsStartupRuntimeModel();
        runtimeModel->SetBounds(float3::get_Zero(), float3::get_Zero());
        runtimeModel->SetSubmeshes(new Array<RuntimeSubmesh*>(0));
        return runtimeModel;
    }

    /// Builds one lightweight runtime material from one platform-owned cooked material asset.
    RuntimeMaterial* Nintendo3DsStartupRenderManager3D::BuildMaterialFromCooked(PlatformMaterialAsset* materialAsset) {
        if (materialAsset == nullptr) {
            throw std::invalid_argument("Nintendo 3DS cooked material asset is required.");
        }

        return new RuntimeMaterial();
    }

    /// Builds one lightweight runtime material from one cooked material asset path.
    RuntimeMaterial* Nintendo3DsStartupRenderManager3D::BuildMaterialFromCooked(std::string cookedAssetPath) {
        if (cookedAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked material asset path is required.");
        }

        return new RuntimeMaterial();
    }

    /// Builds one lightweight runtime material from one raw material asset path.
    RuntimeMaterial* Nintendo3DsStartupRenderManager3D::BuildMaterialFromRawAsset(
        ContentManager* assetContentManager,
        std::string contentRootPath,
        std::string materialAssetPath) {
        if (assetContentManager == nullptr) {
            throw std::invalid_argument("Nintendo 3DS startup material content manager is required.");
        } else if (contentRootPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS startup material content root path is required.");
        } else if (materialAssetPath.empty()) {
            throw std::invalid_argument("Nintendo 3DS startup material asset path is required.");
        }

        return new RuntimeMaterial();
    }
}

#endif
