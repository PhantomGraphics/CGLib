#pragma once

#include "../../Animation/Animation/AnimationClip.h"
#include "../../Animation/Animation/IKSolver.h"
#include "../../Animation/Animation/MorphAnimator.h"
#include "../../Animation/Animation/MorphTarget.h"
#include "../../Animation/Animation/Skeleton.h"

#include <utility>
#include <vector>

namespace Phantom::Gltf {

// Converts Phantom::Animation's sparse, IK-dependent PMX/VMD animation data into dense,
// self-contained keyframe data that can be written directly into glTF animation channels.
// glTF's channels/samplers only interpolate a node's local TRS -- there is no runtime concept of
// solving an IK constraint -- so IK-resolved poses must be baked into keyframes ahead of time
// (see internal design notes Phase 3).
class MmdAnimationBaker {
public:
    // Resamples every bone referenced by any IKChain::chainBones at sampleRateHz, running
    // Animator::computeFK() + IKSolver::solve() at each sample time and reconstructing
    // parent-relative local transforms from the solved global transforms. Bones outside any chain
    // keep their original (possibly sparse) channel unchanged -- IK never touches a bone's own
    // parent-relative local transform unless that bone is itself in a chain; only its world
    // position moves, via the hierarchy, which the runtime evaluator reconstructs on its own.
    static Phantom::Animation::AnimationClip bakeIk(
        const Phantom::Animation::Skeleton& skeleton,
        const std::vector<Phantom::Animation::IKChain>& ikChains,
        const Phantom::Animation::AnimationClip& clip,
        float sampleRateHz = 30.f);

    // Merges every MorphChannel's independent, sparse keyframe times into a single sorted
    // timeline and evaluates the full per-morph weight vector (via MorphAnimator::update()) at
    // each merged time -- matching glTF's "weights" channel convention of one dense array
    // covering every morph target at once, rather than MMD's per-morph independent keys.
    // Returns an empty vector when clip has no channels (no morph animation).
    static std::vector<std::pair<float, std::vector<float>>> bakeMorphWeights(
        const std::vector<Phantom::Animation::MorphTarget>& targets,
        const Phantom::Animation::MorphAnimationClip& clip);
};

} // namespace Phantom::Gltf
