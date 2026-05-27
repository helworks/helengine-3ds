#include "platform/3ds/Nintendo3DsStartupManifestReader.hpp"

#include <cstdio>

namespace helengine::nintendo3ds {
    namespace {
        /// Stores the stable RomFS path used for the 3DS startup manifest payload.
        constexpr const char* StartupManifestPath = "romfs:/runtime/3ds_startup_manifest.bin";

        /// Stores the expected startup-manifest magic.
        constexpr unsigned char StartupManifestMagic[4] = { 0x48, 0x33, 0x53, 0x50 };
    }

    /// Reads and validates the packaged Nintendo 3DS startup manifest from RomFS.
    /// <param name="romFsInitialized">Whether the runtime mounted RomFS successfully before the read attempt.</param>
    /// <returns>Startup-manifest read result.</returns>
    Nintendo3DsStartupManifestReader::Result Nintendo3DsStartupManifestReader::Read(bool romFsInitialized) const {
        if (!romFsInitialized) {
            return { Status::RomFsUnavailable, 0, 0 };
        }

        FILE* file = std::fopen(StartupManifestPath, "rb");
        if (file == nullptr) {
            return { Status::FileMissing, 0, 0 };
        }

        unsigned char magic[4] = {};
        unsigned short version = 0;
        unsigned short payloadSize = 0;
        u32 topScreenColor = 0;
        u32 bottomScreenColor = 0;

        bool readSucceeded = std::fread(magic, sizeof(unsigned char), 4, file) == 4
            && std::fread(&version, sizeof(unsigned short), 1, file) == 1
            && std::fread(&payloadSize, sizeof(unsigned short), 1, file) == 1
            && std::fread(&topScreenColor, sizeof(u32), 1, file) == 1
            && std::fread(&bottomScreenColor, sizeof(u32), 1, file) == 1;
        std::fclose(file);

        if (!readSucceeded) {
            return { Status::InvalidData, 0, 0 };
        } else if (magic[0] != StartupManifestMagic[0] || magic[1] != StartupManifestMagic[1] || magic[2] != StartupManifestMagic[2] || magic[3] != StartupManifestMagic[3]) {
            return { Status::InvalidData, 0, 0 };
        } else if (version != 1) {
            return { Status::InvalidData, 0, 0 };
        } else if (payloadSize < 8) {
            return { Status::InvalidData, 0, 0 };
        }

        return { Status::Success, topScreenColor, bottomScreenColor };
    }
}
