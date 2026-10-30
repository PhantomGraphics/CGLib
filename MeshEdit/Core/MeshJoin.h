#pragma once
// Joins editable meshes into one. Every part must already be in the coordinate system of `base` (the caller transforms positions and
// normals). Vertices, faces, corner UVs / normals and UV seams are carried over unchanged; nothing is welded, cut or repaired, so
// parts that overlap stay overlapping shells. The parts get fresh ids above base's counters; base keeps its ids. The result gets a
// fresh revision. Inputs may alias `out`. Failure (an invalid part, or a combined mesh that does not validate) leaves `out` untouched
// and fills *error. No scene, Vulkan or exception dependency.
#include "EditableMesh.h"

#include <string>
#include <vector>

namespace Phantom::MeshEdit {

bool joinMeshes(const EditableMesh& base, const std::vector<const EditableMesh*>& parts, EditableMesh& out, std::string* error = nullptr);

}
