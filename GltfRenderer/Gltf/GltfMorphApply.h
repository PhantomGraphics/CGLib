#pragma once

#include "GltfDocument.h"

#include <vector>

namespace Phantom::Gltf {

// base + sum(weight[i] * targets[i]) per vertex, for one primitive's POSITION accessor and its
// morph targets (see GltfMorphTarget in GltfTypes.h). weights shorter than prim.targets is
// treated as zero-padded -- mirrors Phantom::Animation::MorphAnimator::applyMorphs()'s same
// convention, just for Gltf-layer types (see internal design notes Phase 7).
// Returns an empty vector if the primitive has no POSITION accessor.
std::vector<glm::vec3> applyMorphs(const GltfDocument& doc, const GltfPrimitive& prim,
                                    const std::vector<float>& weights);

// Same blend for the NORMAL attribute: base normal + sum(weight[i] * target[i].normalAccessor),
// renormalized per vertex. Targets whose normalAccessor is -1 contribute nothing (the caller's
// position blend still moved those verts, but the source gave no normal delta for them).
// Returns an empty vector if the primitive has no NORMAL accessor.
std::vector<glm::vec3> applyMorphedNormals(const GltfDocument& doc, const GltfPrimitive& prim,
                                            const std::vector<float>& weights);

} // namespace Phantom::Gltf
