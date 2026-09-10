#pragma once

#include "GltfDocument.h"

#include <vector>

namespace Phantom::Gltf {

// Evaluates real glTF animation channels/samplers against a GltfDocument's node hierarchy.
// Depends only on GltfDocument -- no Phantom::Animation dependency -- so the same implementation
// works for MMD-derived documents (via MmdToGltfConverter) and any real .gltf/.glb file's own
// animations (see internal design notes Phase 4).
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
    // Without a channel, uses node weights, then mesh weights, then zeros.
    static std::vector<float> evaluateMorphWeights(const GltfDocument& doc,
                                                    int animationIndex, int nodeIndex,
                                                    int targetCount, float timeSec);

    // Per-node global (model-space) transform for every node in doc, with
    // doc.animations[animationIndex]'s Translation/Rotation/Scale channels evaluated at timeSec.
    // Index i is node i; nodes not reached from the default scene stay identity. Pass
    // animationIndex < 0 for the static bind pose. This is the object-animation counterpart of
    // evaluateSkin() -- callers that render whole nodes (not skinned joints) use it to place
    // each mesh (see GltfSceneRenderer's object-animation path).
    static std::vector<glm::mat4> evaluateNodeGlobalTransforms(const GltfDocument& doc,
                                                                int animationIndex, float timeSec);

    // Latest keyframe time across every sampler in anim (0 if it has none).
    static float duration(const GltfAnimation& anim, const GltfDocument& doc);
};

} // namespace Phantom::Gltf
