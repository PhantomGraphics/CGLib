#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../Gltf/MmdAnimationBaker.h"
#include "../../Animation/Animation/Animator.h"

#include <map>

using namespace Phantom::Gltf;
using namespace Phantom::Animation;

namespace {

// Same 4-bone chain as CGLib/Animation/AnimationTest/IKSolverTest.cpp:
// root(0) -> mid(1) -> tip(2), plus a free-standing target bone(3).
Skeleton makeChainSkeleton()
{
    Skeleton sk;

    Bone root;
    root.name            = "Root";
    root.parentIndex     = -1;
    root.localPosition    = {0.f, 0.f, 0.f};
    root.bindPoseInverse  = glm::mat4{1.f};
    sk.addBone(root);

    Bone mid;
    mid.name            = "Mid";
    mid.parentIndex     = 0;
    mid.localPosition    = {0.f, 1.f, 0.f};
    mid.bindPoseInverse  = glm::inverse(glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 1.f, 0.f}));
    sk.addBone(mid);

    Bone tip;
    tip.name            = "Tip";
    tip.parentIndex     = 1;
    tip.localPosition    = {0.f, 1.f, 0.f};
    tip.bindPoseInverse  = glm::inverse(glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 2.f, 0.f}));
    sk.addBone(tip);

    Bone target;
    target.name            = "Target";
    target.parentIndex     = -1;
    target.localPosition    = {1.f, 1.f, 0.f};
    target.bindPoseInverse  = glm::mat4{1.f};
    sk.addBone(target);

    return sk;
}

// Animates the (root) Target bone's position from (1,1,0) at t=0 to (1,2,0) at t=1, so the IK
// solution genuinely differs across sampled times.
AnimationClip makeTargetMotionClip()
{
    AnimationClip clip;
    clip.duration = 1.f;

    BoneChannel ch;
    ch.boneIndex = 3; // Target
    ch.positionKeys.push_back({0.f, glm::vec3{0.f, 0.f, 0.f}}); // delta from bind pose
    ch.positionKeys.push_back({1.f, glm::vec3{0.f, 1.f, 0.f}});
    clip.channels.push_back(ch);
    return clip;
}

IKChain makeChain()
{
    IKChain chain;
    chain.effectorBoneIndex = 2; // Tip
    chain.targetBoneIndex   = 3; // Target
    chain.chainBones        = {1, 0}; // Mid, Root (child -> parent)
    chain.iterationCount    = 20;
    chain.angleLimitRad     = glm::radians(90.f);
    return chain;
}

} // namespace

// -----------------------------------------------------------------------
// bakeIk
// -----------------------------------------------------------------------

TEST(MmdAnimationBakerTest, NoChains_ChannelsPassThroughUnchanged)
{
    const Skeleton sk     = makeChainSkeleton();
    const AnimationClip clip = makeTargetMotionClip();

    const AnimationClip baked = MmdAnimationBaker::bakeIk(sk, {}, clip, 10.f);

    ASSERT_EQ(1u, baked.channels.size());
    EXPECT_EQ(3, baked.channels[0].boneIndex);
    ASSERT_EQ(2u, baked.channels[0].positionKeys.size());
    EXPECT_FLOAT_EQ(0.f, baked.channels[0].positionKeys[0].value.y);
    EXPECT_FLOAT_EQ(1.f, baked.channels[0].positionKeys[1].value.y);
}

TEST(MmdAnimationBakerTest, ChainBonesGetDenseKeyframesMatchingDirectFkIk)
{
    const Skeleton sk         = makeChainSkeleton();
    const AnimationClip clip  = makeTargetMotionClip();
    const IKChain chain       = makeChain();

    const AnimationClip baked = MmdAnimationBaker::bakeIk(sk, {chain}, clip, 10.f);

    // Exactly the two chain bones (Mid=1, Root=0) get baked channels -- not the effector (Tip=2)
    // or the target (3, which keeps its original sparse channel, checked below).
    ASSERT_EQ(3u, baked.channels.size());
    std::map<int, const BoneChannel*> byBone;
    for (const auto& ch : baked.channels) byBone[ch.boneIndex] = &ch;
    ASSERT_TRUE(byBone.count(0));
    ASSERT_TRUE(byBone.count(1));
    ASSERT_TRUE(byBone.count(3));
    EXPECT_EQ(0u, byBone.count(2));

    // Root/Mid should be densely sampled: duration=1s @ 10Hz -> 11 samples (0.0, 0.1, ..., 1.0).
    EXPECT_EQ(11u, byBone[0]->positionKeys.size());
    EXPECT_EQ(11u, byBone[0]->rotationKeys.size());
    EXPECT_EQ(11u, byBone[1]->positionKeys.size());
    EXPECT_EQ(11u, byBone[1]->rotationKeys.size());

    // Target(3) is untouched by IK -- its original sparse channel (2 keys) survives verbatim.
    EXPECT_EQ(2u, byBone[3]->positionKeys.size());

    // Cross-check sample index 5 (t=0.5s) against directly running Animator::computeFK() +
    // IKSolver::solve() and reconstructing local transforms by hand, exactly as bakeIk() itself
    // documents doing (see MmdAnimationBaker.cpp).
    const float t = 0.5f;
    Animator animator;
    animator.computeFK(sk, clip, t);
    std::vector<glm::mat4> globalT = animator.getGlobalTransforms();
    IKSolver solver;
    solver.solve(sk, {chain}, globalT);

    for (int bone : {0, 1}) {
        const int parent = sk.bones[bone].parentIndex;
        const glm::mat4 local = (parent < 0) ? globalT[bone] : glm::inverse(globalT[parent]) * globalT[bone];
        const glm::vec3 expectedTranslationDelta = glm::vec3(local[3]) - sk.bones[bone].localPosition;
        const glm::quat expectedRotation = glm::normalize(glm::quat_cast(glm::mat3(local)));

        // Sample index 5 corresponds to t=0.5 (step 0.1s).
        const auto& posKey = byBone[bone]->positionKeys[5];
        const auto& rotKey = byBone[bone]->rotationKeys[5];
        EXPECT_NEAR(t, posKey.time, 1e-4f);
        EXPECT_NEAR(expectedTranslationDelta.x, posKey.value.x, 1e-5f);
        EXPECT_NEAR(expectedTranslationDelta.y, posKey.value.y, 1e-5f);
        EXPECT_NEAR(expectedTranslationDelta.z, posKey.value.z, 1e-5f);
        EXPECT_NEAR(expectedRotation.x, rotKey.value.x, 1e-5f);
        EXPECT_NEAR(expectedRotation.y, rotKey.value.y, 1e-5f);
        EXPECT_NEAR(expectedRotation.z, rotKey.value.z, 1e-5f);
        EXPECT_NEAR(expectedRotation.w, rotKey.value.w, 1e-5f);
    }
}

TEST(MmdAnimationBakerTest, EmptyClip_ProducesOneSampleAtZero)
{
    const Skeleton sk = makeChainSkeleton();
    AnimationClip clip; // duration=0, no channels
    const IKChain chain = makeChain();

    const AnimationClip baked = MmdAnimationBaker::bakeIk(sk, {chain}, clip, 10.f);

    std::map<int, const BoneChannel*> byBone;
    for (const auto& ch : baked.channels) byBone[ch.boneIndex] = &ch;
    ASSERT_TRUE(byBone.count(0));
    EXPECT_EQ(1u, byBone[0]->positionKeys.size());
    EXPECT_FLOAT_EQ(0.f, byBone[0]->positionKeys[0].time);
}

// -----------------------------------------------------------------------
// bakeMorphWeights
// -----------------------------------------------------------------------

TEST(MmdAnimationBakerTest, BakeMorphWeights_EmptyClipReturnsEmpty)
{
    std::vector<MorphTarget> targets(2);
    MorphAnimationClip clip; // no channels

    const auto baked = MmdAnimationBaker::bakeMorphWeights(targets, clip);
    EXPECT_TRUE(baked.empty());
}

TEST(MmdAnimationBakerTest, BakeMorphWeights_MergesSparseTimelinesMatchingMorphAnimator)
{
    std::vector<MorphTarget> targets(2); // deltas irrelevant to weight evaluation

    MorphAnimationClip clip;
    MorphChannel ch0;
    ch0.morphIndex = 0;
    ch0.keyframes  = {{0.f, 0.f}, {1.f, 1.f}};
    MorphChannel ch1;
    ch1.morphIndex = 1;
    ch1.keyframes  = {{0.5f, 0.3f}}; // single key -> constant 0.3 everywhere
    clip.channels  = {ch0, ch1};

    const auto baked = MmdAnimationBaker::bakeMorphWeights(targets, clip);

    // Union of key times across both channels: {0.0, 0.5, 1.0}.
    ASSERT_EQ(3u, baked.size());
    EXPECT_FLOAT_EQ(0.0f, baked[0].first);
    EXPECT_FLOAT_EQ(0.5f, baked[1].first);
    EXPECT_FLOAT_EQ(1.0f, baked[2].first);

    MorphAnimator animator;
    MorphState    state;
    for (const auto& [time, weights] : baked) {
        animator.update(targets, clip, time, state);
        ASSERT_EQ(state.weights.size(), weights.size());
        for (size_t i = 0; i < weights.size(); ++i)
            EXPECT_NEAR(state.weights[i], weights[i], 1e-6f);
    }
}
