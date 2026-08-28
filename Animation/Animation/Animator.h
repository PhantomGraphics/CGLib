#pragma once

#include "Skeleton.h"
#include "AnimationClip.h"

#include <vector>

namespace Phantom::Animation {

// Evaluates skeletal animation pose at a given time.
// For IK support, split the evaluation:
//   1. computeFK()          → fills globalTransforms_
//   2. IKSolver::solve()    → modifies globalTransforms_ in place
//   3. computeSkinMatrices() → fills skinMatrices_ from globalTransforms_
// update() is a convenience wrapper that calls both without IK.
class Animator {
public:
    // Forward kinematics only: fills globalTransforms_ from skeleton + clip.
    void computeFK(const Skeleton& skeleton, const AnimationClip& clip, float timeSec);

    // Compute skinning matrices from the current globalTransforms_.
    // Call this after computeFK() (and after IKSolver::solve() if IK is used).
    void computeSkinMatrices(const Skeleton& skeleton);

    // Convenience: computeFK() + computeSkinMatrices() (no IK).
    void update(const Skeleton& skeleton, const AnimationClip& clip, float timeSec);

    const std::vector<glm::mat4>& getSkinMatrices()     const { return skinMatrices_; }
    const std::vector<glm::mat4>& getGlobalTransforms()  const { return globalTransforms_; }
    std::vector<glm::mat4>&       getGlobalTransforms()        { return globalTransforms_; }

private:
    std::vector<glm::mat4> skinMatrices_;
    std::vector<glm::mat4> globalTransforms_;

    static glm::vec3 interpolatePosition(const BoneChannel& ch, float time);
    static glm::quat interpolateRotation(const BoneChannel& ch, float time);
    static glm::vec3 interpolateScale   (const BoneChannel& ch, float time);

    static glm::mat4 buildLocalTransform(const glm::vec3& pos,
                                          const glm::quat& rot,
                                          const glm::vec3& scale);
};

} // namespace Phantom::Animation
