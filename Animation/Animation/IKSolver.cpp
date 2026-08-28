#include "IKSolver.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace Phantom::Animation {

void IKSolver::solve(const Skeleton&             sk,
                     const std::vector<IKChain>& chains,
                     std::vector<glm::mat4>&     globalT)
{
    const int n = static_cast<int>(sk.bones.size());
    if (n == 0 || chains.empty()) return;

    // Precompute parent-relative local transforms.
    // These are updated incrementally as each bone is rotated.
    std::vector<glm::mat4> localT(n, glm::mat4{1.f});
    for (int i = 0; i < n; ++i) {
        const int p = sk.bones[i].parentIndex;
        localT[i] = (p < 0) ? globalT[i] : glm::inverse(globalT[p]) * globalT[i];
    }

    for (const auto& chain : chains) {
        if (chain.effectorBoneIndex < 0 || chain.effectorBoneIndex >= n) continue;
        if (chain.targetBoneIndex   < 0 || chain.targetBoneIndex   >= n) continue;
        if (chain.chainBones.empty()) continue;

        for (int iter = 0; iter < chain.iterationCount; ++iter) {
            const glm::vec3 targetPos = glm::vec3(globalT[chain.targetBoneIndex][3]);

            for (int ci = 0; ci < static_cast<int>(chain.chainBones.size()); ++ci) {
                const int boneIdx = chain.chainBones[ci];
                if (boneIdx < 0 || boneIdx >= n) continue;

                const glm::vec3 bonePos     = glm::vec3(globalT[boneIdx][3]);
                const glm::vec3 effectorPos = glm::vec3(globalT[chain.effectorBoneIndex][3]);

                glm::vec3 toEff = effectorPos - bonePos;
                glm::vec3 toTgt = targetPos   - bonePos;

                const float lenEff = glm::length(toEff);
                const float lenTgt = glm::length(toTgt);
                if (lenEff < 1e-6f || lenTgt < 1e-6f) continue;
                toEff /= lenEff;
                toTgt /= lenTgt;

                const float cosA  = glm::clamp(glm::dot(toEff, toTgt), -1.f, 1.f);
                float       angle = std::acos(cosA);
                if (angle < 1e-6f) continue;
                angle = std::min(angle, chain.angleLimitRad);

                glm::vec3 axis = glm::cross(toEff, toTgt);
                const float axisLen = glm::length(axis);
                if (axisLen < 1e-6f) continue;
                axis /= axisLen;

                // Rotate bone about its joint position in world space.
                // The translation column stays at bonePos; the rotation part gets pre-multiplied by dR.
                const glm::mat4 dR = glm::rotate(glm::mat4{1.f}, angle, axis);
                globalT[boneIdx] =
                    glm::translate(glm::mat4{1.f},  bonePos) *
                    dR *
                    glm::translate(glm::mat4{1.f}, -bonePos) *
                    globalT[boneIdx];

                // Update local transform for the modified bone.
                {
                    const int p = sk.bones[boneIdx].parentIndex;
                    localT[boneIdx] = (p < 0) ? globalT[boneIdx]
                                              : glm::inverse(globalT[p]) * globalT[boneIdx];
                }

                // Propagate to chain bones closer to the effector (indices 0..ci-1).
                for (int k = ci - 1; k >= 0; --k) {
                    const int ci_k  = chain.chainBones[k];
                    const int par_k = sk.bones[ci_k].parentIndex;
                    globalT[ci_k] =
                        (par_k < 0 ? glm::mat4{1.f} : globalT[par_k]) * localT[ci_k];
                }
                // Propagate to effector bone.
                {
                    const int ei  = chain.effectorBoneIndex;
                    const int eip = sk.bones[ei].parentIndex;
                    globalT[ei] =
                        (eip < 0 ? glm::mat4{1.f} : globalT[eip]) * localT[ei];
                }
            }
        }
    }
}

} // namespace Phantom::Animation
