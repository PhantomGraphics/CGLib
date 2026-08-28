#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include "MorphTarget.h"
#include "SkinnedMesh.h"

#include <string>
#include <utility>
#include <vector>

namespace Phantom::Animation {

// --- MorphAnimationClip -------------------------------------------------

struct MorphChannel {
    int                                  morphIndex = -1;
    std::vector<std::pair<float, float>> keyframes; // (timeSec, weight), sorted by time
};

struct MorphAnimationClip {
    std::string               name;
    float                     duration = 0.f;
    std::vector<MorphChannel> channels;
};

// --- MorphAnimator ------------------------------------------------------

// Evaluates morph weights from a MorphAnimationClip and applies position
// deltas to a SkinnedMesh.
class MorphAnimator {
public:
    // Interpolate per-morph weights at timeSec -> state.weights.
    // state.weights is resized to targets.size() and reset to 0 on each call.
    void update(const std::vector<MorphTarget>& targets,
                const MorphAnimationClip&       clip,
                float                           timeSec,
                MorphState&                     state);

    // Apply morph deltas: outMesh = baseMesh + sum(weights[i] * targets[i].deltas).
    static void applyMorphs(const std::vector<SkinVertex>&  baseMesh,
                            const std::vector<MorphTarget>& targets,
                            const MorphState&               state,
                            std::vector<SkinVertex>&        outMesh);
};

} // namespace Phantom::Animation
