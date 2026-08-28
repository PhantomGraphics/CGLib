#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../Animation/Bone.h"
#include "../Animation/Skeleton.h"
#include "../Animation/AnimationClip.h"
#include "../Animation/Animator.h"

using namespace Phantom::Animation;

namespace {

// Build a minimal 2-bone skeleton: root (index 0) and child (index 1).
// Both bones start at the origin in bind pose.
Skeleton makeTwoBoneskelton() {
    Skeleton sk;

    Bone root;
    root.name        = "Root";
    root.parentIndex = -1;
    root.bindPoseInverse = glm::mat4{1.f}; // identity = bind pose at origin
    sk.addBone(root);

    Bone child;
    child.name          = "Child";
    child.parentIndex   = 0;
    child.localPosition = {0.f, 1.f, 0.f}; // 1 unit above root in bind pose
    // bindPoseInverse = inverse of global bind-pose transform = inverse(T(0,1,0))
    child.bindPoseInverse = glm::inverse(
        glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 1.f, 0.f}));
    sk.addBone(child);

    return sk;
}

} // namespace

// -----------------------------------------------------------------------
// Skeleton helpers
// -----------------------------------------------------------------------

TEST(SkeletonTest, RootBoneIndices) {
    Skeleton sk = makeTwoBoneskelton();
    auto roots = sk.rootBoneIndices();
    ASSERT_EQ(roots.size(), 1u);
    EXPECT_EQ(roots[0], 0);
}

TEST(SkeletonTest, BoneNameToIndex) {
    Skeleton sk = makeTwoBoneskelton();
    EXPECT_EQ(sk.boneNameToIndex["Root"],  0);
    EXPECT_EQ(sk.boneNameToIndex["Child"], 1);
}

// -----------------------------------------------------------------------
// Animator – T-pose (no animation clip)
// -----------------------------------------------------------------------

TEST(AnimatorTest, TPoseIdentityClip) {
    Skeleton sk = makeTwoBoneskelton();

    AnimationClip clip;
    clip.name     = "empty";
    clip.duration = 1.f;

    Animator anim;
    anim.update(sk, clip, 0.f);

    const auto& mats = anim.getSkinMatrices();
    ASSERT_EQ(mats.size(), 2u);

    // Root bone: global = identity, skinMatrix = identity * identity = identity
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            EXPECT_NEAR(mats[0][col][row], glm::mat4{1.f}[col][row], 1e-5f);

    // Child bone: global = T(0,1,0), skinMatrix = T(0,1,0) * T(0,-1,0) = identity
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            EXPECT_NEAR(mats[1][col][row], glm::mat4{1.f}[col][row], 1e-5f);
}

// -----------------------------------------------------------------------
// Animator – simple arm rotation animation
// -----------------------------------------------------------------------

TEST(AnimatorTest, ChildRotation90DegZ) {
    Skeleton sk = makeTwoBoneskelton();

    // Animate child bone: rotate 90 deg around Z from t=0 to t=1
    AnimationClip clip;
    clip.name     = "arm_rotate";
    clip.duration = 1.f;

    BoneChannel ch;
    ch.boneIndex = 1; // Child

    // Position: no offset from bind pose (0,0,0) -- position keys are additive deltas (see
    // Animator::computeFK()), so this keeps the child at its bind-pose (0,1,0) local position.
    ch.positionKeys.push_back({0.f, glm::vec3{0.f, 0.f, 0.f}});
    ch.positionKeys.push_back({1.f, glm::vec3{0.f, 0.f, 0.f}});

    // Rotation: 0 deg → 90 deg around Z
    ch.rotationKeys.push_back({0.f, glm::quat{1.f, 0.f, 0.f, 0.f}});
    ch.rotationKeys.push_back({1.f, glm::angleAxis(glm::radians(90.f), glm::vec3{0.f, 0.f, 1.f})});

    ch.scaleKeys.push_back({0.f, glm::vec3{1.f}});
    ch.scaleKeys.push_back({1.f, glm::vec3{1.f}});

    clip.channels.push_back(ch);

    Animator anim;
    anim.update(sk, clip, 1.f); // evaluate at t=1

    const auto& mats = anim.getSkinMatrices();

    // Root skin matrix should still be identity
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            EXPECT_NEAR(mats[0][col][row], glm::mat4{1.f}[col][row], 1e-5f);

    // Child global transform: T(0,1,0) * R(90 Z)
    // Skin matrix = global * bindPoseInverse = global * T(0,-1,0)
    // Expected: T(0,1,0) * R(90Z) * T(0,-1,0)
    // A point at (0,-1,0) in local (bind-pose child space) maps to:
    //   T(0,1,0): (0, 0, 0) → after rotation: still (0,0,0)
    // Numerically verify the [3] column (translation part) of skinMatrix
    glm::vec4 trans = mats[1][3];
    EXPECT_NEAR(trans.x, 1.f, 1e-5f); // R(90Z) maps (0,-1,0) → (1,0,0), then +T(0,1,0)
    EXPECT_NEAR(trans.y, 1.f, 1e-5f);
    EXPECT_NEAR(trans.z, 0.f, 1e-5f);
}

// -----------------------------------------------------------------------
// Animator – mid-frame interpolation
// -----------------------------------------------------------------------

TEST(AnimatorTest, MidFrameInterpolation) {
    Skeleton sk = makeTwoBoneskelton();

    AnimationClip clip;
    clip.name     = "half";
    clip.duration = 1.f;

    BoneChannel ch;
    ch.boneIndex = 0; // Root
    ch.positionKeys.push_back({0.f, glm::vec3{0.f, 0.f, 0.f}});
    ch.positionKeys.push_back({1.f, glm::vec3{2.f, 0.f, 0.f}});
    clip.channels.push_back(ch);

    Animator anim;
    anim.update(sk, clip, 0.5f);

    const auto& mats = anim.getSkinMatrices();
    // Root at t=0.5: position = (1,0,0), bindPoseInverse = identity
    EXPECT_NEAR(mats[0][3][0], 1.f, 1e-5f);
    EXPECT_NEAR(mats[0][3][1], 0.f, 1e-5f);
    EXPECT_NEAR(mats[0][3][2], 0.f, 1e-5f);
}
