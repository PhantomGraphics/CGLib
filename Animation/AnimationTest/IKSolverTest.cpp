#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../Animation/IKSolver.h"
#include "../Animation/Skeleton.h"

using namespace Phantom::Animation;

namespace {

// 3-bone chain: root(0) -> mid(1) -> tip(2), plus a target bone(3).
// Bind pose: root at (0,0,0), mid at (0,1,0), tip at (0,2,0), target at (1,1,0)
Skeleton makeChainSkeleton()
{
    Skeleton sk;

    Bone root;
    root.name          = "Root";
    root.parentIndex   = -1;
    root.localPosition = {0.f, 0.f, 0.f};
    root.localRotation = glm::quat{1.f, 0.f, 0.f, 0.f};
    root.localScale    = glm::vec3{1.f};
    root.bindPoseInverse = glm::mat4{1.f};
    sk.addBone(root);

    Bone mid;
    mid.name          = "Mid";
    mid.parentIndex   = 0;
    mid.localPosition = {0.f, 1.f, 0.f};
    mid.localRotation = glm::quat{1.f, 0.f, 0.f, 0.f};
    mid.localScale    = glm::vec3{1.f};
    mid.bindPoseInverse = glm::inverse(glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 1.f, 0.f}));
    sk.addBone(mid);

    Bone tip;
    tip.name          = "Tip";
    tip.parentIndex   = 1;
    tip.localPosition = {0.f, 1.f, 0.f};
    tip.localRotation = glm::quat{1.f, 0.f, 0.f, 0.f};
    tip.localScale    = glm::vec3{1.f};
    tip.bindPoseInverse = glm::inverse(glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 2.f, 0.f}));
    sk.addBone(tip);

    Bone target;
    target.name          = "Target";
    target.parentIndex   = -1;
    target.localPosition = {1.f, 1.f, 0.f};
    target.localRotation = glm::quat{1.f, 0.f, 0.f, 0.f};
    target.localScale    = glm::vec3{1.f};
    target.bindPoseInverse = glm::mat4{1.f};
    sk.addBone(target);

    return sk;
}

std::vector<glm::mat4> makeBindPoseGlobalT()
{
    std::vector<glm::mat4> gt(4, glm::mat4{1.f});
    gt[0] = glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.f, 0.f});
    gt[1] = glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 1.f, 0.f});
    gt[2] = glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 2.f, 0.f});
    gt[3] = glm::translate(glm::mat4{1.f}, glm::vec3{1.f, 1.f, 0.f});
    return gt;
}

} // namespace

// -----------------------------------------------------------------------
// IKSolver - effector moves toward target after solve
// -----------------------------------------------------------------------

TEST(IKSolverTest, EffectorMovesTowardTarget)
{
    Skeleton sk = makeChainSkeleton();
    auto globalT = makeBindPoseGlobalT();

    const glm::vec3 targetPos     = glm::vec3(globalT[3][3]);
    const glm::vec3 effectorBefore = glm::vec3(globalT[2][3]);
    const float     distBefore    = glm::length(effectorBefore - targetPos);

    IKChain chain;
    chain.effectorBoneIndex = 2; // Tip
    chain.targetBoneIndex   = 3; // Target
    chain.chainBones        = {1, 0}; // Mid, Root (child -> parent)
    chain.iterationCount    = 20;
    chain.angleLimitRad     = glm::radians(90.f);

    IKSolver solver;
    solver.solve(sk, {chain}, globalT);

    const glm::vec3 effectorAfter = glm::vec3(globalT[2][3]);
    const float     distAfter     = glm::length(effectorAfter - targetPos);

    EXPECT_LT(distAfter, distBefore) << "IK should move effector closer to target";
}

// -----------------------------------------------------------------------
// IKSolver - no chains: global transforms unchanged
// -----------------------------------------------------------------------

TEST(IKSolverTest, NoChains_NoChange)
{
    Skeleton sk = makeChainSkeleton();
    auto globalT  = makeBindPoseGlobalT();
    auto expected = globalT;

    IKSolver solver;
    solver.solve(sk, {}, globalT);

    for (int i = 0; i < 4; ++i)
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                EXPECT_NEAR(globalT[i][c][r], expected[i][c][r], 1e-5f);
}

// -----------------------------------------------------------------------
// IKSolver - angle limit constrains rotation per step
// -----------------------------------------------------------------------

TEST(IKSolverTest, AngleLimitConstrainsRotation)
{
    Skeleton sk = makeChainSkeleton();

    // Target far to the side: large rotation needed
    auto globalT = makeBindPoseGlobalT();
    globalT[3] = glm::translate(glm::mat4{1.f}, glm::vec3{5.f, 1.f, 0.f});

    IKChain chain;
    chain.effectorBoneIndex = 2;
    chain.targetBoneIndex   = 3;
    chain.chainBones        = {1, 0};
    chain.iterationCount    = 1;
    chain.angleLimitRad     = glm::radians(5.f); // small limit

    auto globalT_limited   = globalT;
    auto globalT_unlimited = globalT;

    IKSolver solver;
    solver.solve(sk, {chain}, globalT_limited);

    chain.angleLimitRad = glm::radians(180.f);
    solver.solve(sk, {chain}, globalT_unlimited);

    const glm::vec3 tgt           = glm::vec3(globalT[3][3]);
    const float     distLimited   = glm::length(glm::vec3(globalT_limited[2][3])   - tgt);
    const float     distUnlimited = glm::length(glm::vec3(globalT_unlimited[2][3]) - tgt);

    // Limited rotation should not beat unlimited rotation in a single iteration
    EXPECT_GE(distLimited, distUnlimited - 1e-4f);
}

// -----------------------------------------------------------------------
// IKSolver - invalid bone indices are ignored safely
// -----------------------------------------------------------------------

TEST(IKSolverTest, InvalidBoneIndexIgnored)
{
    Skeleton sk = makeChainSkeleton();
    auto globalT = makeBindPoseGlobalT();

    IKChain chain;
    chain.effectorBoneIndex = 99; // out of range
    chain.targetBoneIndex   = 3;
    chain.chainBones        = {1, 0};
    chain.iterationCount    = 5;

    IKSolver solver;
    EXPECT_NO_FATAL_FAILURE(solver.solve(sk, {chain}, globalT));
}

// -----------------------------------------------------------------------
// IKSolver - target at current effector position: no change needed
// -----------------------------------------------------------------------

TEST(IKSolverTest, AlreadyAtTarget_MinimalChange)
{
    Skeleton sk = makeChainSkeleton();
    auto globalT = makeBindPoseGlobalT();

    // Move target to the tip's current position
    globalT[3] = globalT[2];

    IKChain chain;
    chain.effectorBoneIndex = 2;
    chain.targetBoneIndex   = 3;
    chain.chainBones        = {1, 0};
    chain.iterationCount    = 5;
    chain.angleLimitRad     = glm::radians(90.f);

    IKSolver solver;
    solver.solve(sk, {chain}, globalT);

    const glm::vec3 eff = glm::vec3(globalT[2][3]);
    const glm::vec3 tgt = glm::vec3(globalT[3][3]);
    EXPECT_NEAR(glm::length(eff - tgt), 0.f, 0.1f);
}
