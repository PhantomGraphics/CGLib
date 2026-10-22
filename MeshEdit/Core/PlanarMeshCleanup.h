#pragma once
#include "EditableMesh.h"
#include <cstddef>

namespace Phantom::MeshEdit {
struct PlanarCleanupOptions {
    double relativeTolerance = 1e-7; // relative to input AABB diagonal
    double attributeTolerance = 1e-6; // UV/normal interpolation, relative + absolute
    size_t maxWork = 50000000;
    // These edges and EditableMesh::seams must survive unchanged.
    std::vector<std::pair<uint32_t, uint32_t>> protectedEdges;
};
// CPU-only cleanup of a validated triangle mesh. Removes redundant vertices by
// retriangulating planar, attribute-compatible vertex fans. Each fan is a disk,
// so holes and separate shells require no global polygon stitching. Creases and
// UV/normal interpolation boundaries survive. No positions are moved or added.
// Output may alias input; failure leaves it untouched. Unchanged results retain
// their revision. Changed faces receive fresh IDs; surviving vertices keep IDs.
bool cleanupPlanarMesh(const EditableMesh& input, EditableMesh& output,
                       const PlanarCleanupOptions& options = {}, std::string* error = nullptr);
}
