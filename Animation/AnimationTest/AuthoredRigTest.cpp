#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <limits>
#include <string>

#include "../Core/AuthoredRig.h"

using namespace Phantom::Animation;

namespace {

// Three-bone chain along +Y: A (0..1) -> B (1..2) -> C (2..3).
AuthoredRig makeChain(uint32_t* ids = nullptr)
{
    AuthoredRig rig;
    auto a = rig.addBone("A", {0, 0, 0}, {0, 1, 0});
    auto b = rig.addChainBone(a.id, "B", {0, 2, 0});
    auto c = rig.addChainBone(b.id, "C", {0, 3, 0});
    if (ids) { ids[0] = a.id; ids[1] = b.id; ids[2] = c.id; }
    return rig;
}

} // namespace

TEST(AuthoredRigTest, AddBoneAssignsStableIdsAndRevision)
{
    AuthoredRig rig;
    const uint64_t r0 = rig.revision();
    auto a = rig.addBone("A", {0, 0, 0}, {0, 1, 0});
    ASSERT_TRUE(a.ok);
    EXPECT_NE(a.id, 0u);
    EXPECT_GT(rig.revision(), r0);
    auto b = rig.addBone("B", {1, 0, 0}, {1, 1, 0});
    EXPECT_NE(a.id, b.id);
    ASSERT_TRUE(rig.removeBone(a.id).ok);
    auto c = rig.addBone("C", {2, 0, 0}, {2, 1, 0});
    EXPECT_NE(c.id, a.id); // ids are never reused
}

TEST(AuthoredRigTest, ChainBoneStartsAtParentTail)
{
    uint32_t ids[3];
    AuthoredRig rig = makeChain(ids);
    const AuthoredBone* b = rig.find(ids[1]);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->connected);
    EXPECT_EQ(b->parent, ids[0]);
    EXPECT_EQ(b->head, glm::vec3(0, 1, 0));
}

TEST(AuthoredRigTest, RejectsInvalidInputAndLeavesRigUnchanged)
{
    AuthoredRig rig = makeChain();
    const uint64_t rev = rig.revision();
    const size_t n = rig.size();
    const float nan = std::numeric_limits<float>::quiet_NaN();

    EXPECT_FALSE(rig.addBone("A", {5, 0, 0}, {5, 1, 0}).ok);              // duplicate name
    EXPECT_FALSE(rig.addBone("", {5, 0, 0}, {5, 1, 0}).ok);               // empty name
    EXPECT_FALSE(rig.addBone("Z", {1, 1, 1}, {1, 1, 1}).ok);              // zero length
    EXPECT_FALSE(rig.addBone("Z", {nan, 0, 0}, {1, 1, 1}).ok);            // non finite
    EXPECT_FALSE(rig.addBone("Z", {0, 0, 0}, {0, 1, 0}, 0.f, 999).ok);    // missing parent
    EXPECT_FALSE(rig.addBone("Z", {0, 0, 0}, {0, 1, 0}, nan).ok);         // non finite roll
    EXPECT_EQ(rig.size(), n);
    EXPECT_EQ(rig.revision(), rev);
}

TEST(AuthoredRigTest, RejectsCycles)
{
    uint32_t ids[3];
    AuthoredRig rig = makeChain(ids);
    EXPECT_FALSE(rig.setParent(ids[0], ids[2]).ok);
    EXPECT_FALSE(rig.setParent(ids[0], ids[0]).ok);
    EXPECT_EQ(rig.find(ids[0])->parent, 0u);
    EXPECT_TRUE(rig.validate().ok);
}

TEST(AuthoredRigTest, SetParentKeepsRestPlacementAndDisconnects)
{
    uint32_t ids[3];
    AuthoredRig rig = makeChain(ids);
    ASSERT_TRUE(rig.setParent(ids[2], ids[0]).ok);
    const AuthoredBone* c = rig.find(ids[2]);
    EXPECT_EQ(c->head, glm::vec3(0, 2, 0));
    EXPECT_FALSE(c->connected); // A's tail is (0,1,0), not C's head
}

TEST(AuthoredRigTest, RemoveReparentsChildren)
{
    uint32_t ids[3];
    AuthoredRig rig = makeChain(ids);
    ASSERT_TRUE(rig.removeBone(ids[1]).ok);
    EXPECT_EQ(rig.size(), 2u);
    EXPECT_EQ(rig.find(ids[2])->parent, ids[0]);
    EXPECT_FALSE(rig.find(ids[2])->connected);
    EXPECT_FALSE(rig.removeBone(ids[1]).ok);
}

TEST(AuthoredRigTest, RenameDuplicateAndUniqueNames)
{
    uint32_t ids[3];
    AuthoredRig rig = makeChain(ids);
    EXPECT_FALSE(rig.renameBone(ids[0], "B").ok);
    ASSERT_TRUE(rig.renameBone(ids[0], "Root").ok);
    EXPECT_NE(rig.findByName("Root"), nullptr);
    auto d1 = rig.duplicateBone(ids[2]);
    auto d2 = rig.duplicateBone(ids[2]);
    ASSERT_TRUE(d1.ok);
    ASSERT_TRUE(d2.ok);
    EXPECT_EQ(rig.find(d1.id)->name, "C.001");
    EXPECT_EQ(rig.find(d2.id)->name, "C.002");
}

TEST(AuthoredRigTest, SetBoneMovesConnectedChildHeadAndProtectsConnectedHead)
{
    uint32_t ids[3];
    AuthoredRig rig = makeChain(ids);
    ASSERT_TRUE(rig.setBone(ids[0], {0, 0, 0}, {0, 1.5f, 0}, 0.f).ok);
    EXPECT_EQ(rig.find(ids[1])->head, glm::vec3(0, 1.5f, 0));
    EXPECT_TRUE(rig.validate().ok);
    EXPECT_FALSE(rig.setBone(ids[1], {1, 1.5f, 0}, {1, 2, 0}, 0.f).ok);
    // Moving a tail onto a connected child's tail would make it zero length -> rejected, unchanged.
    EXPECT_FALSE(rig.setBone(ids[0], {0, 0, 0}, {0, 2, 0}, 0.f).ok);
    EXPECT_EQ(rig.find(ids[0])->tail, glm::vec3(0, 1.5f, 0));
}

TEST(AuthoredRigTest, BoneCountLimit)
{
    AuthoredRig rig;
    for (size_t i = 0; i < AuthoredRig::MaxBones; ++i)
        ASSERT_TRUE(rig.addBone("b" + std::to_string(i), {float(i), 0, 0}, {float(i), 1, 0}).ok);
    EXPECT_FALSE(rig.addBone("overflow", {0, 5, 0}, {0, 6, 0}).ok);
    EXPECT_EQ(rig.size(), AuthoredRig::MaxBones);
}

TEST(AuthoredRigTest, RestMatrixAlignsYAxisWithHeadToTail)
{
    AuthoredBone b;
    b.head = {1, 2, 3};
    b.tail = {1, 2, 5};
    const glm::mat4 m = AuthoredRig::restMatrix(b);
    EXPECT_NEAR(glm::length(glm::vec3(m[3]) - b.head), 0.f, 1e-6f);
    const glm::vec3 y = glm::normalize(glm::vec3(m[1]));
    EXPECT_NEAR(glm::dot(y, glm::vec3(0, 0, 1)), 1.f, 1e-5f);
    // Antiparallel direction is well defined too.
    b.tail = {1, 0, 3};
    const glm::mat4 m2 = AuthoredRig::restMatrix(b);
    EXPECT_NEAR(glm::dot(glm::normalize(glm::vec3(m2[1])), glm::vec3(0, -1, 0)), 1.f, 1e-5f);
    EXPECT_NEAR(glm::determinant(m2), 1.f, 1e-5f);
}

TEST(AuthoredRigTest, RollRotatesAboutBoneAxis)
{
    AuthoredBone b;
    b.head = {0, 0, 0};
    b.tail = {0, 1, 0};
    const glm::mat4 m0 = AuthoredRig::restMatrix(b);
    b.roll = glm::half_pi<float>();
    const glm::mat4 m1 = AuthoredRig::restMatrix(b);
    EXPECT_NEAR(glm::length(glm::vec3(m0[1]) - glm::vec3(m1[1])), 0.f, 1e-5f); // axis unchanged
    EXPECT_NEAR(glm::dot(glm::vec3(m0[0]), glm::vec3(m1[0])), 0.f, 1e-5f);     // X turned a quarter
}

TEST(AuthoredRigTest, ToSkeletonParentFirstAndForwardKinematicsMatchRest)
{
    AuthoredRig rig;
    // Insert out of hierarchical order: child first, then re-parent.
    auto c = rig.addBone("C", {0, 2, 0}, {0, 3, 0});
    auto a = rig.addBone("A", {0, 0, 0}, {0.5f, 1, 0}, 0.3f);
    auto b = rig.addBone("B", {0.5f, 1, 0}, {0.5f, 2, 0.5f}, -0.7f, a.id, true);
    ASSERT_TRUE(rig.setParent(c.id, b.id).ok);

    Skeleton sk;
    std::vector<uint32_t> idx;
    ASSERT_TRUE(rig.toSkeleton(sk, &idx).ok);
    ASSERT_EQ(sk.bones.size(), 3u);
    std::vector<glm::mat4> world(3);
    for (size_t i = 0; i < 3; ++i) {
        const Bone& bone = sk.bones[i];
        ASSERT_LT(bone.parentIndex, static_cast<int>(i)); // parents first
        glm::mat4 local = glm::translate(glm::mat4(1.f), bone.localPosition) * glm::mat4_cast(bone.localRotation);
        world[i] = bone.parentIndex < 0 ? local : world[bone.parentIndex] * local;
        const glm::mat4 expected = AuthoredRig::restMatrix(*rig.find(idx[i]));
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                EXPECT_NEAR(world[i][col][row], expected[col][row], 1e-5f);
        // Skinning at rest is the identity.
        const glm::mat4 skin = world[i] * bone.bindPoseInverse;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                EXPECT_NEAR(skin[col][row], col == row ? 1.f : 0.f, 1e-5f);
    }
    EXPECT_EQ(sk.boneNameToIndex.at("A"), 0);
}

TEST(AuthoredRigTest, EmptyRigConvertsToEmptySkeleton)
{
    AuthoredRig rig;
    Skeleton sk;
    EXPECT_TRUE(rig.toSkeleton(sk).ok);
    EXPECT_TRUE(sk.bones.empty());
}
