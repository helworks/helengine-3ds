#include "platform/3ds/Nintendo3DsPackagedAssetLoader.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <cstdio>
#include <stdexcept>

#include "Asset.hpp"
#include "AssetSerializer.hpp"
#include "SceneAsset.hpp"
#include "system/io/file.hpp"

namespace helengine::nintendo3ds {
    /// Normalizes one RomFS root path to forward slashes without a trailing separator.
    std::string Nintendo3DsPackagedAssetLoader::NormalizeRootPath(const std::string& value) {
        if (value.empty()) {
            throw std::invalid_argument("Nintendo 3DS content root path is required.");
        }

        std::string normalized = value;
        for (std::size_t index = 0; index < normalized.size(); index++) {
            if (normalized[index] == '\\') {
                normalized[index] = '/';
            }
        }

        while (normalized.size() > 1 && normalized.back() == '/') {
            normalized.pop_back();
        }

        return normalized;
    }

    /// Normalizes one cooked-relative path to forward slashes.
    std::string Nintendo3DsPackagedAssetLoader::NormalizeRelativePath(const std::string& value) {
        if (value.empty()) {
            throw std::invalid_argument("Nintendo 3DS cooked-relative asset path is required.");
        }

        std::string normalized = value;
        for (std::size_t index = 0; index < normalized.size(); index++) {
            if (normalized[index] == '\\') {
                normalized[index] = '/';
            }
        }

        if (normalized[0] == '/') {
            throw std::invalid_argument("Nintendo 3DS cooked-relative asset path must not be rooted.");
        } else if (normalized.find("..") != std::string::npos) {
            throw std::invalid_argument("Nintendo 3DS cooked-relative asset path must stay inside RomFS.");
        }

        return normalized;
    }

    /// Creates one packaged asset loader for the supplied RomFS content root.
    Nintendo3DsPackagedAssetLoader::Nintendo3DsPackagedAssetLoader(const std::string& contentRootPath)
        : ContentRootPath(NormalizeRootPath(contentRootPath)) {
    }

    /// Builds one full RomFS asset path from the normalized root and cooked-relative path.
    std::string Nintendo3DsPackagedAssetLoader::BuildContentPath(const std::string& cookedRelativePath) const {
        std::string normalizedRelativePath = NormalizeRelativePath(cookedRelativePath);
        return ContentRootPath + "/" + normalizedRelativePath;
    }

    /// Determines whether one packaged asset exists inside RomFS.
    bool Nintendo3DsPackagedAssetLoader::AssetExists(const std::string& cookedRelativePath) const {
        std::string fullPath = BuildContentPath(cookedRelativePath);
        FILE* file = std::fopen(fullPath.c_str(), "rb");
        if (file == nullptr) {
            return false;
        }

        std::fclose(file);
        return true;
    }

    /// Loads one packaged asset using a cooked-relative path.
    Asset* Nintendo3DsPackagedAssetLoader::LoadAsset(const std::string& cookedRelativePath) const {
        std::string fullPath = BuildContentPath(cookedRelativePath);
        FileStream* stream = nullptr;
        try {
            stream = File::OpenRead(fullPath);
        } catch (const std::exception& exception) {
            throw std::runtime_error("Packaged Nintendo 3DS asset could not be opened: " + fullPath + " (" + exception.what() + ")");
        }

        if (stream == nullptr) {
            throw std::runtime_error("Packaged Nintendo 3DS asset could not be opened: " + fullPath);
        }

        Asset* asset = AssetSerializer::Deserialize(stream);
        delete stream;
        return asset;
    }

    /// Loads one packaged scene asset using a cooked-relative path.
    SceneAsset* Nintendo3DsPackagedAssetLoader::LoadSceneAsset(const std::string& cookedRelativePath) const {
        Asset* asset = LoadAsset(cookedRelativePath);
        SceneAsset* sceneAsset = dynamic_cast<SceneAsset*>(asset);
        if (sceneAsset == nullptr) {
            throw std::runtime_error("Packaged Nintendo 3DS asset was not one scene asset: " + cookedRelativePath);
        }

        return sceneAsset;
    }
}

#endif
