#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Skeleton.h"
#include <vector>

namespace Phantom::Animation {

// IK chain descriptor.
// chainBones is ordered child -> parent (chainBones[0] is closest to effector).
struct IKChain {
    int              effectorBoneIndex = -1; // bone tip that must reach target
    int              targetBoneIndex   = -1; // IK controller (desired position)
    std::vector<int> chainBones;            // bones to rotate, child -> parent
    int              iterationCount = 10;
    float            angleLimitRad  = 0.5f; // max rotation per bone per iteration (~28.6 deg)
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
