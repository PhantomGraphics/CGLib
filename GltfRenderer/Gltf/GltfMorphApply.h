#pragma once

#include "GltfDocument.h"

#include <vector>

namespace Phantom::Gltf {

// base + sum(weight[i] * targets[i]) per vertex, for one primitive's POSITION accessor and its
// morph targets (see GltfMorphTarget in GltfTypes.h). weights shorter than prim.targets is
// treated as zero-padded -- mirrors Phantom::Animation::MorphAnimator::applyMorphs()'s same
// convention, just for Gltf-layer types (see docs/todo/PLAN_mmd_gltf_unification.md Phase 7).
// Returns an empty vector if the primitive has no POSITION accessor.
std::vector<glm::vec3> applyMorphs(const GltfDocument& doc, const GltfPrimitive& prim,
                                    const std::vector<float>& weights);

} // namespace Phantom::Gltf
