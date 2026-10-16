#pragma once
// CPU solid Boolean operations. Inputs use a common coordinate system and outward
// oriented closed surfaces (cavity shells face into the cavity). Input quads must
// be planar within tolerance. Empty meshes represent the empty set.
// Output is triangulated, with fresh ids above a's counters and a fresh revision.
// Inputs may alias out. Failure leaves out untouched and reports a reason; success
// clears *error. No scene, Vulkan, external CSG library, or exception dependency.
#include "EditableMesh.h"
#include <cstddef>

namespace Phantom::MeshEdit {
enum class BooleanOperation { Union, Difference, Intersection };
struct BooleanOptions {
    // Length tolerance relative to the diagonal of the combined input AABB.
    // Supported range: 1e-12..1e-3. Arithmetic is double; output positions are float.
    double relativeTolerance = 1e-7;
    size_t maxInputTriangles = 100000;
    size_t maxIntermediatePolygons = 1000000; // cumulative input + split fragments
    size_t maxOutputFaces = 1000000;
    // Traversal additionally has a fixed 50-million-work budget and BSP depth 512.
};
bool booleanMeshes(const EditableMesh& a, const EditableMesh& b, BooleanOperation operation,
                   EditableMesh& out, const BooleanOptions& options = {}, std::string* error = nullptr);
} // namespace Phantom::MeshEdit
