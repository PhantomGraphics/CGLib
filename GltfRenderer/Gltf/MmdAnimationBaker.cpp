#include "MmdAnimationBaker.h"

#include "../../Animation/Animation/Animator.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>

#include <set>
#include <unordered_set>

using namespace Phantom::Animation;

namespace Phantom::Gltf {

namespace {

// Dense sample times covering [0, duration] at sampleRateHz, always including the exact end time
// (a plain fixed-step loop can undershoot it by rounding error, leaving the final pose unbaked).
std::vector<float> buildSampleTimes(float duration, float sampleRateHz) {
    std::vector<float> times;
    if (duration <= 0.f || sampleRateHz <= 0.f) {
        times.push_back(0.f);
        return times;
    }
    const float step = 1.f / sampleRateHz;
    for (float t = 0.f; t < duration; t += step)
        times.push_back(t);
    if (times.empty() || times.back() < duration)
        times.push_back(duration);
    return times;
}

} // namespace

AnimationClip MmdAnimationBaker::bakeIk(const Skeleton& skeleton,
                                         const std::vector<IKChain>& ikChains,
                                         const AnimationClip& clip,
                                         float sampleRateHz)
{
    AnimationClip baked;
    baked.name          = clip.name;
    baked.duration       = clip.duration;
    baked.ticksPerSecond = clip.ticksPerSecond;

    const int boneCount = static_cast<int>(skeleton.bones.size());

    std::unordered_set<int> bakedBones;
    for (const auto& chain : ikChains)
        for (int b : chain.chainBones)
            if (b >= 0 && b < boneCount)
                bakedBones.insert(b);

    if (bakedBones.empty()) {
        baked.channels = clip.channels;
        return baked;
    }

    // Pre-allocate one channel per baked bone up front so the sample loop below can hold raw
    // pointers into baked.channels without risking reallocation.
    std::vector<BoneChannel*> outChannel(boneCount, nullptr);
    baked.channels.reserve(bakedBones.size());
    for (int b : bakedBones) {
        BoneChannel ch;
        ch.boneIndex = b;
        baked.channels.push_back(std::move(ch));
    }
    for (auto& ch : baked.channels)
        outChannel[ch.boneIndex] = &ch;

    const std::vector<float> sampleTimes = buildSampleTimes(clip.duration, sampleRateHz);

    Animator animator;
    IKSolver  solver;
    for (float t : sampleTimes) {
        animator.computeFK(skeleton, clip, t);
        std::vector<glm::mat4> globalT = animator.getGlobalTransforms();
        solver.solve(skeleton, ikChains, globalT);

        for (int b : bakedBones) {
            const int parent = skeleton.bones[b].parentIndex;
            const glm::mat4 local = (parent < 0)
                ? globalT[b]
                : glm::inverse(globalT[parent]) * globalT[b];

            const glm::vec3 translation = glm::vec3(local[3]);
            const glm::quat rotation    = glm::normalize(glm::quat_cast(glm::mat3(local)));

            BoneChannel& ch = *outChannel[b];
            // Stored as a delta from the bind pose, matching Animator::computeFK()'s own
            // convention for position keys (Animator.cpp: bone.localPosition + interpolated key).
            ch.positionKeys.push_back({t, translation - skeleton.bones[b].localPosition});
            ch.rotationKeys.push_back({t, rotation});
        }
    }

    // Bones untouched by IK keep whatever (possibly sparse, possibly absent) channel they
    // already had -- no rebaking needed, see class comment.
    for (const auto& ch : clip.channels) {
        if (bakedBones.count(ch.boneIndex)) continue;
        baked.channels.push_back(ch);
    }

    return baked;
}

std::vector<std::pair<float, std::vector<float>>>
MmdAnimationBaker::bakeMorphWeights(const std::vector<MorphTarget>& targets,
                                     const MorphAnimationClip& clip)
{
    std::vector<std::pair<float, std::vector<float>>> result;
    if (clip.channels.empty()) return result;

    std::set<float> times;
    for (const auto& ch : clip.channels)
        for (const auto& kf : ch.keyframes)
            times.insert(kf.first);
    if (times.empty()) return result;

    result.reserve(times.size());
    MorphAnimator animator;
    MorphState    state;
    for (float t : times) {
        animator.update(targets, clip, t, state);
        result.push_back({t, state.weights});
    }
    return result;
}

} // namespace Phantom::Gltf
