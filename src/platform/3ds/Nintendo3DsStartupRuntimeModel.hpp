#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include "RuntimeModel.hpp"

namespace helengine::nintendo3ds {
    /// Exposes one public runtime-model constructor for startup-scene materialization without backend-owned mesh state.
    class Nintendo3DsStartupRuntimeModel : public RuntimeModel {
    public:
        /// Creates one lightweight runtime model placeholder for startup-scene materialization.
        Nintendo3DsStartupRuntimeModel();
    };
}

#endif
