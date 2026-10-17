#pragma once

namespace helengine::nintendo3ds {
    /// Writes a citro3d-compatible affine view matrix without decomposing shared sprite corners.
    class Nintendo3DsSpriteGeometry {
    public:
        /// Maps unit-image corners onto the supplied signed basis plus the active viewport offset.
        template<class Matrix>
        static void WriteUnitQuadMatrix(Matrix& matrix, float tlX, float tlY, float trX, float trY,
            float blX, float blY, float offsetX, float offsetY) {
            matrix = {};
            matrix.r[0].x = trX - tlX;
            matrix.r[0].y = blX - tlX;
            matrix.r[0].w = tlX + offsetX;
            matrix.r[1].x = trY - tlY;
            matrix.r[1].y = blY - tlY;
            matrix.r[1].w = tlY + offsetY;
            matrix.r[2].z = 1.0f;
            matrix.r[3].w = 1.0f;
        }
    };
}
