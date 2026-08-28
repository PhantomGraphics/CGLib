#pragma once

#include "GltfDocument.h"

#include <vector>

namespace Phantom::Gltf {

// Evaluates real glTF animation channels/samplers against a GltfDocument's node hierarchy.
// Depends only on GltfDocument -- no Phantom::Animation dependency -- so the same implementation
// works for MMD-derived documents (via MmdToGltfConverter) and any real .gltf/.glb file's own
// animations (see docs/todo/PLAN_mmd_gltf_unification.md Phase 4).
class GltfAnimationEvaluator {
public:
    // Evaluates doc.animations[animationIndex]'s Translation/Rotation/Scale channels at timeSec,
    // walks the node hierarchy from doc.scenes[doc.defaultScene] to get each node's global
    // transform, then combines each of doc.skins[skinIndex]'s joints' global transform with its
    // inverseBindMatrices entry -- the same convention GltfSceneRenderer::updateSkinMatrices()
    // expects (see GltfSkin in GltfTypes.h). Times before/after the animation's range clamp to
    // the first/last keyframe. Returns an empty vector if skinIndex is out of range.
    static std::vector<glm::mat4> evaluateSkin(const GltfDocument& doc,
                                                int animationIndex, int skinIndex, float timeSec);

    // Evaluates doc.animations[animationIndex]'s Weights channel targeting nodeIndex at timeSec.
    // Returns targetCount zeros (= no deformation) if no such channel exists.
    static std::vector<float> evaluateMorphWeights(const GltfDocument& doc,
                                                    int animationIndex, int nodeIndex,
                                                    int targetCount, float timeSec);

    // Latest keyframe time across every sampler in anim (0 if it has none).
    static float duration(const GltfAnimation& anim, const GltfDocument& doc);
};

} // namespace Phantom::Gltf
