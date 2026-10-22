#pragma once
#include "EditableMesh.h"
#include <cstddef>

namespace Phantom::MeshEdit {
enum class ReductionStop { TargetReached, ProtectedTopology };
struct MeshReductionOptions {
    // Set targetTriangles > 0 to use a count instead of ratio. The ratio must
    // remain its default in count mode, avoiding ambiguous requests.
    double ratio = 0.5;
    size_t targetTriangles = 0;
    size_t maxInputTriangles = 1000000;
    size_t maxWork = 50000000;
};
struct MeshReductionReport {
    size_t inputVertices = 0, outputVertices = 0;
    size_t inputTriangles = 0, outputTriangles = 0, targetTriangles = 0;
    size_t collapses = 0, work = 0;
    bool changed = false, targetReached = false;
    ReductionStop stop = ReductionStop::ProtectedTopology;
};
// Conservative, deterministic QEM edge collapse in object-local space. Triangles
// and planar quads are accepted; changed results contain triangles. Borders,
// seams, discontinuous corner attributes and >=45 degree features have fixed
// endpoints. Components and holes are retained; intersections are rejected.
// Neither input nor output/report is modified on failure (aliasing is allowed).
// A target blocked by constraints is a successful partial result; unchanged
// results retain the original topology, IDs and revision. No scene/renderer use.
bool reduceMesh(const EditableMesh& input, const MeshReductionOptions& options,
                EditableMesh& output, MeshReductionReport& report, std::string* error = nullptr);
}
