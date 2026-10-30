#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Skeleton.h"
#include <vector>

namespace Phantom::Animation {

// Rotation limit of one chain bone: the euler angles (YXZ order, radians; the order `pose` rotation uses) of the bone's rotation
// relative to its REST local rotation (Bone::localRotation) stay inside [minRad, maxRad] per axis. enabled = false means free.
struct IKJointLimit {
    bool      enabled = false;
    glm::vec3 minRad{0.f};
    glm::vec3 maxRad{0.f};
};

// IK chain descriptor.
// chainBones is ordered child -> parent (chainBones[0] is closest to effector).
struct IKChain {
    int              effectorBoneIndex = -1; // bone tip that must reach target
    int              targetBoneIndex   = -1; // IK controller (desired position)
    std::vector<int> chainBones;            // bones to rotate, child -> parent
    int              iterationCount = 10;
    float            angleLimitRad  = 0.5f; // max rotation per bone per iteration (~28.6 deg)
    std::vector<IKJointLimit> limits;       // aligned with chainBones (child -> parent); empty or short = free joints
};

// CCD-IK solver. Operates on world-space global transforms.
// Call after Animator::computeFK(), before Animator::computeSkinMatrices().
class IKSolver {
public:
    void solve(const Skeleton&             skeleton,
               const std::vector<IKChain>& chains,
               std::vector<glm::mat4>&     globalTransforms);
};

} // namespace Phantom::Animation
