#include "MorphAnimator.h"

#include <glm/glm.hpp>
#include <algorithm>

namespace Phantom::Animation {

void MorphAnimator::update(const std::vector<MorphTarget>& targets,
                            const MorphAnimationClip&       clip,
                            float                           timeSec,
                            MorphState&                     state)
{
    const int n = static_cast<int>(targets.size());
    state.weights.assign(n, 0.f);

    for (const auto& ch : clip.channels) {
        if (ch.morphIndex < 0 || ch.morphIndex >= n) continue;
        if (ch.keyframes.empty()) continue;

        float w = 0.f;
        if (timeSec <= ch.keyframes.front().first) {
            w = ch.keyframes.front().second;
        } else if (timeSec >= ch.keyframes.back().first) {
            w = ch.keyframes.back().second;
        } else {
            auto it = std::lower_bound(ch.keyframes.begin(), ch.keyframes.end(),
                std::pair<float, float>{timeSec, 0.f});
            const auto [t1, w1] = *it;
            const auto [t0, w0] = *(it - 1);
            const float alpha = (timeSec - t0) / (t1 - t0);
            w = glm::mix(w0, w1, alpha);
        }
        state.weights[ch.morphIndex] = glm::clamp(w, 0.f, 1.f);
    }
}

void MorphAnimator::applyMorphs(const std::vector<SkinVertex>&  baseMesh,
                                 const std::vector<MorphTarget>& targets,
                                 const MorphState&               state,
                                 std::vector<SkinVertex>&        outMesh)
{
    outMesh = baseMesh;
    const int numTargets = static_cast<int>(targets.size());

    for (int ti = 0; ti < numTargets; ++ti) {
        const float w = (ti < static_cast<int>(state.weights.size())) ? state.weights[ti] : 0.f;
        if (w <= 0.f) continue;
        for (const auto& delta : targets[ti].deltas) {
            if (delta.vertexIndex >= 0 &&
                delta.vertexIndex < static_cast<int>(outMesh.size()))
            {
                outMesh[delta.vertexIndex].position += w * delta.positionOffset;
            }
        }
    }
}

} // namespace Phantom::Animation
