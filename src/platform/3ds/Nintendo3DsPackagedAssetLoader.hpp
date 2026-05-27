#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <string>

class Asset;
class SceneAsset;

namespace helengine::nintendo3ds {
    /// Loads packaged generated-core assets from the mounted Nintendo 3DS RomFS root.
    class Nintendo3DsPackagedAssetLoader {
    public:
        /// Creates one packaged asset loader for the supplied RomFS content root.
        /// <param name="contentRootPath">RomFS content root used to resolve packaged assets.</param>
        explicit Nintendo3DsPackagedAssetLoader(const std::string& contentRootPath);

        /// Determines whether one packaged asset exists inside RomFS.
        /// <param name="cookedRelativePath">Cooked-relative asset path inside RomFS.</param>
        /// <returns>True when the packaged asset can be opened from RomFS.</returns>
        bool AssetExists(const std::string& cookedRelativePath) const;

        /// Loads one packaged asset using a cooked-relative path.
        /// <param name="cookedRelativePath">Cooked-relative asset path inside RomFS.</param>
        /// <returns>Deserialized packaged asset.</returns>
        Asset* LoadAsset(const std::string& cookedRelativePath) const;

        /// Loads one packaged scene asset using a cooked-relative path.
        /// <param name="cookedRelativePath">Cooked-relative scene asset path inside RomFS.</param>
        /// <returns>Deserialized packaged scene asset.</returns>
        SceneAsset* LoadSceneAsset(const std::string& cookedRelativePath) const;

    private:
        /// Stores the normalized RomFS content root used to resolve packaged assets.
        std::string ContentRootPath;

        /// Builds one full RomFS asset path from the normalized root and cooked-relative path.
        /// <param name="cookedRelativePath">Cooked-relative asset path inside RomFS.</param>
        /// <returns>Full RomFS asset path.</returns>
        std::string BuildContentPath(const std::string& cookedRelativePath) const;

        /// Normalizes one RomFS root path to forward slashes without a trailing separator.
        /// <param name="value">Authored content-root path.</param>
        /// <returns>Normalized RomFS content-root path.</returns>
        static std::string NormalizeRootPath(const std::string& value);

        /// Normalizes one cooked-relative path to forward slashes.
        /// <param name="value">Authored cooked-relative asset path.</param>
        /// <returns>Normalized cooked-relative asset path.</returns>
        static std::string NormalizeRelativePath(const std::string& value);
    };
}

#endif
