#include "platform/3ds/Nintendo3DsRuntimeModel.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <3ds.h>

namespace helengine::nintendo3ds {
    /// Creates one Nintendo 3DS runtime model with one linear-allocated vertex stream ready for citro3d submission.
    Nintendo3DsRuntimeModel::Nintendo3DsRuntimeModel(float3* vertexData, int32_t vertexCount)
        : RuntimeModel()
        , VertexData(vertexData)
        , VertexCount(vertexCount) {
    }

    /// Releases the Nintendo 3DS-owned linear vertex buffer before the runtime model is discarded.
    void Nintendo3DsRuntimeModel::Dispose() {
        if (VertexData != nullptr) {
            linearFree(VertexData);
            VertexData = nullptr;
        }

        VertexCount = 0;
        RuntimeModel::Dispose();
    }

    /// Gets the linear-allocated vertex data used by the Nintendo 3DS solid-color path.
    float3* Nintendo3DsRuntimeModel::GetVertexData() const {
        return VertexData;
    }

    /// Gets the number of vertices stored in the linear-allocated triangle stream.
    int32_t Nintendo3DsRuntimeModel::GetVertexCount() const {
        return VertexCount;
    }
}

#endif
