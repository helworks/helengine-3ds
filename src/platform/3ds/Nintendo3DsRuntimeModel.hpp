#pragma once

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <cstdint>

#include "RuntimeModel.hpp"
#include "platform/3ds/Nintendo3DsModelVertex.hpp"

namespace helengine::nintendo3ds {
    /// Stores one Nintendo 3DS-owned runtime model copy so mesh components can render after the source model asset has been released by the scene loader.
    class Nintendo3DsRuntimeModel final : public RuntimeModel {
    public:
        /// Creates one Nintendo 3DS runtime model with one linear-allocated vertex stream ready for citro3d submission.
        /// <param name="vertexData">Linear-allocated triangle-stream vertex data owned by this runtime model.</param>
        /// <param name="untexturedVertexData">Linear-allocated packed position-and-normal stream owned by this runtime model.</param>
        /// <param name="vertexCount">Number of vertices stored in the supplied triangle streams.</param>
        Nintendo3DsRuntimeModel(Nintendo3DsModelVertex* vertexData, Nintendo3DsUntexturedModelVertex* untexturedVertexData, int32_t vertexCount);

        /// Releases the Nintendo 3DS-owned linear vertex buffer before the runtime model is discarded.
        void Dispose() override;

        /// Gets the linear-allocated vertex data used by the Nintendo 3DS lit-color path.
        Nintendo3DsModelVertex* GetVertexData() const;

        /// Gets the packed position-and-normal stream used by the Nintendo 3DS untextured lit-color path.
        Nintendo3DsUntexturedModelVertex* GetUntexturedVertexData() const;

        /// Gets the number of vertices stored in the linear-allocated triangle stream.
        int32_t GetVertexCount() const;

    private:
        /// Stores the linear-allocated triangle-stream vertex data consumed by citro3d.
        Nintendo3DsModelVertex* VertexData;

        /// Stores the linear-allocated packed position-and-normal stream consumed by citro3d.
        Nintendo3DsUntexturedModelVertex* UntexturedVertexData;

        /// Stores the number of vertices contained in the linear-allocated triangle stream.
        int32_t VertexCount;
    };
}

#endif
