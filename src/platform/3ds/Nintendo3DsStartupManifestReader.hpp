#pragma once

#include <3ds.h>

namespace helengine::nintendo3ds {
    /// Loads and validates the builder-owned Nintendo 3DS startup manifest from RomFS.
    class Nintendo3DsStartupManifestReader {
    public:
        /// Describes the possible RomFS startup-manifest read outcomes.
        enum class Status {
            /// RomFS could not be mounted by the runtime.
            RomFsUnavailable,

            /// The startup-manifest file was not found.
            FileMissing,

            /// The startup-manifest file could not be parsed.
            InvalidData,

            /// The startup-manifest file was read successfully.
            Success
        };

        /// Stores one startup-manifest read result.
        struct Result {
            /// Stores the read outcome.
            Status ReadStatus;

            /// Stores the top-screen clear color read from the manifest.
            u32 TopScreenColor;

            /// Stores the bottom-screen clear color read from the manifest.
            u32 BottomScreenColor;
        };

        /// Reads and validates the packaged Nintendo 3DS startup manifest from RomFS.
        /// <param name="romFsInitialized">Whether the runtime mounted RomFS successfully before the read attempt.</param>
        /// <returns>Startup-manifest read result.</returns>
        Result Read(bool romFsInitialized) const;
    };
}
