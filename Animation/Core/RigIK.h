#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AuthoredRig.h"
#include "IKSolver.h"

#include <vector>

namespace Phantom::Animation {

// Single-chain inverse kinematics for an authored rig (an arm or a leg), on top of the CCD IKSolver.
//
// The chain is a run of bones from the root of the chain to its tip (each bone's parent is the one before it). The effector is the TAIL of
// the last bone. The result is one pose rotation per chain bone, expressed the way the Studio stores a static pose: the rotation of
// `local = restLocal * delta`. The solver is stateless: the same request gives the same result and nothing depends on another frame.

struct RigIKRequest {
    const AuthoredRig*       rig = nullptr;
    std::vector<uint32_t>    chain;            // bone ids, chain root first
    glm::mat4                parentGlobal{1.f}; // global matrix (rig space) of the parent of the chain root; identity when it has none
    std::vector<glm::mat4>   delta;            // current pose matrix (translation * rotation * scale) of each chain bone; empty = identity
    std::vector<IKJointLimit> limits;          // per chain bone, chain root first; empty or short = free joints
    glm::vec3                target{0.f};      // rig space
    int                      iterations = 60;  // CCD sweeps
    float                    stepRad = 0.5f;   // largest rotation per bone per sweep (separate from the joint limits)
    float                    reachTolerance = 1.0e-3f;
};

struct RigIKResult {
    RigResult              status;
    std::vector<glm::quat> rotation;   // new pose rotation per chain bone, chain root first
    glm::vec3              effector{0.f}; // effector position (rig space) after the solve
    float                  distance = 0.f; // |effector - target|
    bool                   reached = false; // distance <= reachTolerance
};

namespace RigIK {

// Validates the chain (known bones, each the child of the one before, no repeats, at most 16 bones, finite inputs) and solves.
// A target that cannot be reached still gives the best finite pose found (status ok, reached = false, distance = what is left).
RigIKResult solve(const RigIKRequest& request);

} // namespace RigIK

} // namespace Phantom::Animation
