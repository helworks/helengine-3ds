#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <string>

class FileStream;

namespace helengine::nintendo3ds {
    /// Provides read-only packaged RomFS access for generated-core file reads on Nintendo 3DS.
    class Nintendo3DsRomFsFileSystem final {
    public:
        /// Returns whether the supplied path should be resolved from the Nintendo 3DS RomFS mount.
        static bool CanHandlePath(const char* path);

        /// Returns whether the supplied RomFS path resolves to one packaged file.
        static bool Exists(const char* path);

        /// Opens one packaged RomFS file as one read-only memory-backed stream.
        static FileStream* OpenRead(const char* path);

    private:
        /// Normalizes one candidate RomFS path into the slash form expected by libctru stdio hooks.
        static std::string NormalizePath(const char* path);
    };
}

#endif
