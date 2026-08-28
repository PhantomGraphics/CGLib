#include "Animator.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cassert>
#include <stack>

namespace Phantom::Animation {

// --- keyframe interpolation helpers -----------------------------------------

glm::vec3 Animator::interpolatePosition(const BoneChannel& ch, float time) {
    const auto& keys = ch.positionKeys;
    if (keys.empty()) return glm::vec3{0.f};
    if (keys.size() == 1) return keys[0].value;
    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time)  return keys.back().value;

    // Binary search for the pair [k0, k1] that straddles time
    auto it = std::lower_bound(keys.begin(), keys.end(), time,
        [](const Vec3Key& k, float t){ return k.time < t; });
    const Vec3Key& k1 = *it;
    const Vec3Key& k0 = *(it - 1);
    float t = (time - k0.time) / (k1.time - k0.time);
    return glm::mix(k0.value, k1.value, t);
}

glm::quat Animator::interpolateRotation(const BoneChannel& ch, float time) {
    const auto& keys = ch.rotationKeys;
    if (keys.empty()) return glm::quat{1.f, 0.f, 0.f, 0.f};
    if (keys.size() == 1) return keys[0].value;
    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time)  return keys.back().value;

    auto it = std::lower_bound(keys.begin(), keys.end(), time,
        [](const QuatKey& k, float t){ return k.time < t; });
    const QuatKey& k1 = *it;
    const QuatKey& k0 = *(it - 1);
    float t = (time - k0.time) / (k1.time - k0.time);
    return glm::normalize(glm::slerp(k0.value, k1.value, t));
}

glm::vec3 Animator::interpolateScale(const BoneChannel& ch, float time) {
    const auto& keys = ch.scaleKeys;
    if (keys.empty()) return glm::vec3{1.f};
    if (keys.size() == 1) return keys[0].value;
    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time)  return keys.back().value;

    auto it = std::lower_bound(keys.begin(), keys.end(), time,
        [](const Vec3Key& k, float t){ return k.time < t; });
    const Vec3Key& k1 = *it;
    const Vec3Key& k0 = *(it - 1);
    float t = (time - k0.time) / (k1.time - k0.time);
    return glm::mix(k0.value, k1.value, t);
}

glm::mat4 Animator::buildLocalTransform(const glm::vec3& pos,
                                         const glm::quat& rot,
                                         const glm::vec3& scale) {
    glm::mat4 T = glm::translate(glm::mat4{1.f}, pos);
    glm::mat4 R = glm::mat4_cast(rot);
    glm::mat4 S = glm::scale(glm::mat4{1.f}, scale);
    return T * R * S;
}

// --- Animator::computeFK ----------------------------------------------------

void Animator::computeFK(const Skeleton& skeleton, const AnimationClip& clip, float timeSec) {
    const int n = static_cast<int>(skeleton.bones.size());
    globalTransforms_.resize(n, glm::mat4{1.f});
    skinMatrices_.resize(n, glm::mat4{1.f}); // ensure consistent size

    std::vector<const BoneChannel*> channelMap(n, nullptr);
    for (const auto& ch : clip.channels) {
        if (ch.boneIndex >= 0 && ch.boneIndex < n)
            channelMap[ch.boneIndex] = &ch;
    }

    // Process in index order (parent index < child index by convention).
    for (int i = 0; i < n; ++i) {
        const Bone& bone = skeleton.bones[i];

        glm::mat4 local;
        if (channelMap[i]) {
            const BoneChannel& ch = *channelMap[i];
            // Position keys are a delta from the bind pose, not an absolute replacement -- VMD
            // stores (0,0,0) for any bone that isn't being translated (the overwhelming majority;
            // most channels only carry rotation), and bone.localPosition is the bone's actual
            // parent-relative bind-pose offset (its "length"). Treating the key as absolute would
            // zero out that offset and collapse the bone onto its parent. Rotation/scale keys are
            // still used as absolute local values: PMXConverter always sets bind-pose
            // localRotation/localScale to identity, so "absolute" and "relative to bind" coincide.
            local = buildLocalTransform(
                bone.localPosition + interpolatePosition(ch, timeSec),
                interpolateRotation(ch, timeSec),
                interpolateScale   (ch, timeSec));
        } else {
            local = buildLocalTransform(
                bone.localPosition, bone.localRotation, bone.localScale);
        }

        globalTransforms_[i] = (bone.parentIndex == -1)
            ? local
            : globalTransforms_[bone.parentIndex] * local;
    }
}

// --- Animator::computeSkinMatrices ------------------------------------------

void Animator::computeSkinMatrices(const Skeleton& skeleton) {
    const int n = static_cast<int>(skeleton.bones.size());
    skinMatrices_.resize(n, glm::mat4{1.f});
    for (int i = 0; i < n; ++i)
        skinMatrices_[i] = globalTransforms_[i] * skeleton.bones[i].bindPoseInverse;
}

// --- Animator::update -------------------------------------------------------

void Animator::update(const Skeleton& skeleton, const AnimationClip& clip, float timeSec) {
    computeFK(skeleton, clip, timeSec);
    computeSkinMatrices(skeleton);
}

} // namespace Phantom::Animation
