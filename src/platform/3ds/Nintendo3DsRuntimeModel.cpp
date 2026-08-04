#include "platform/3ds/Nintendo3DsRuntimeModel.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <3ds.h>

namespace helengine::nintendo3ds {
    /// Creates one Nintendo 3DS runtime model with linear-allocated streams ready for citro3d submission.
    Nintendo3DsRuntimeModel::Nintendo3DsRuntimeModel(Nintendo3DsModelVertex* vertexData, Nintendo3DsUntexturedModelVertex* untexturedVertexData, int32_t vertexCount)
        : RuntimeModel()
        , VertexData(vertexData)
        , UntexturedVertexData(untexturedVertexData)
        , VertexCount(vertexCount) {
    }

    /// Releases the Nintendo 3DS-owned linear vertex buffer before the runtime model is discarded.
    void Nintendo3DsRuntimeModel::Dispose() {
        if (VertexData != nullptr) {
            linearFree(VertexData);
            VertexData = nullptr;
        }
        if (UntexturedVertexData != nullptr) {
            linearFree(UntexturedVertexData);
            UntexturedVertexData = nullptr;
        }

        VertexCount = 0;
        RuntimeModel::Dispose();
    }

    /// Gets the linear-allocated vertex data used by the Nintendo 3DS lit-color path.
    Nintendo3DsModelVertex* Nintendo3DsRuntimeModel::GetVertexData() const {
        return VertexData;
    }

    /// Gets the packed position-and-normal stream used by the Nintendo 3DS untextured lit-color path.
    Nintendo3DsUntexturedModelVertex* Nintendo3DsRuntimeModel::GetUntexturedVertexData() const {
        return UntexturedVertexData;
    }

    /// Gets the number of vertices stored in the linear-allocated triangle stream.
    int32_t Nintendo3DsRuntimeModel::GetVertexCount() const {
        return VertexCount;
    }
}

#endif
