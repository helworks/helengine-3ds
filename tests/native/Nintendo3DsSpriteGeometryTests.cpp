#include "platform/3ds/Nintendo3DsSpriteGeometry.hpp"
#include <stdexcept>
#include <cstdio>

/// Exercises the production matrix writer using the public C3D row/component shape.
int main() {
    struct Row { float x, y, z, w; };
    struct Matrix { Row r[4]; } matrix {};
    helengine::nintendo3ds::Nintendo3DsSpriteGeometry::WriteUnitQuadMatrix(matrix, 80, 10, 0, 30, 100, 70, 5, 7);
    const float x[4] = {0, 1, 1, 0}, y[4] = {0, 0, 1, 1};
    const float expectedX[4] = {85, 5, 25, 105}, expectedY[4] = {17, 37, 97, 77};
    for (int i = 0; i < 4; ++i) {
        const float actualX = matrix.r[0].x * x[i] + matrix.r[0].y * y[i] + matrix.r[0].w;
        const float actualY = matrix.r[1].x * x[i] + matrix.r[1].y * y[i] + matrix.r[1].w;
        if (actualX != expectedX[i] || actualY != expectedY[i]) { throw std::runtime_error("3DS affine matrix changed corner identity"); }
    }
    if (matrix.r[2].z != 1 || matrix.r[3].w != 1) { throw std::runtime_error("3DS affine matrix changed homogeneous depth"); }
    std::puts("PASS 3DS unit-image matrix preserves sheared/reflected corners and viewport offset");
}
