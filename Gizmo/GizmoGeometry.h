#pragma once
#include <vector>
#include <cstdint>

namespace Phantom::Gizmo {

struct GizmoVertex {
    float pos[3];
};

struct DrawRange {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
};

// Packed VBO/IBO for all 3 gizmo modes.
// All geometry is generated in the +Z direction.
// The model matrix in the push constant rotates it to each axis.
struct GizmoBufferData {
    std::vector<GizmoVertex> vertices;
    std::vector<uint32_t>    indices;

    DrawRange translate; // shaft(cylinder) + head(cone)
    DrawRange rotate;    // flat ring
    DrawRange scale;     // shaft(cylinder) + cube handle
};

// Builds the packed VBO/IBO for all 3 gizmo modes.
// Asserts vertex/index counts for self-verification.
GizmoBufferData buildGizmoBuffers();

} // namespace Phantom::Gizmo
