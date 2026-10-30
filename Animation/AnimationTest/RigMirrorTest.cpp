#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <string>

#include "../Core/RigMirror.h"

using namespace Phantom::Animation;

namespace {

// A left arm chain with a roll on the first bone, hanging off a centre Spine.
struct ArmRig {
    AuthoredRig rig;
    uint32_t spine = 0, upper = 0, lower = 0, hand = 0;
};

ArmRig makeArm() {
    ArmRig a;
    a.spine = a.rig.addBone("Spine", {0, 1, 0}, {0, 2, 0}).id;
    a.upper = a.rig.addBone("Upper.L", {0.2f, 1.8f, 0.1f}, {1.2f, 1.9f, 0.1f}, 0.5f, a.spine).id;
    a.lower = a.rig.addChainBone(a.upper, "Lower.L", {2.0f, 1.9f, 0.3f}).id;
    a.hand = a.rig.addChainBone(a.lower, "Hand.L", {2.4f, 1.9f, 0.3f}).id;
    return a;
}

} // namespace

TEST(RigMirrorTest, MirrorsPlacementRollNamesAndHierarchy) {
    ArmRig a = makeArm();
    const MirrorResult m = RigMirror::mirrorBones(a.rig, {a.upper});
    ASSERT_TRUE(m.status) << m.status.error;
    ASSERT_EQ(m.pairs.size(), 3u);
    EXPECT_EQ(a.rig.size(), 7u);

    const AuthoredBone* upper = a.rig.findByName("Upper.R");
    const AuthoredBone* lower = a.rig.findByName("Lower.R");
    const AuthoredBone* hand = a.rig.findByName("Hand.R");
    ASSERT_TRUE(upper && lower && hand);
    EXPECT_EQ(upper->head, glm::vec3(-0.2f, 1.8f, 0.1f));
    EXPECT_EQ(upper->tail, glm::vec3(-1.2f, 1.9f, 0.1f));
    EXPECT_FLOAT_EQ(upper->roll, -0.5f);
    EXPECT_EQ(upper->parent, a.spine) << "a centre parent is shared, not mirrored";
    EXPECT_EQ(lower->parent, upper->id);
    EXPECT_EQ(hand->parent, lower->id);
    EXPECT_TRUE(lower->connected);
    EXPECT_TRUE(hand->connected);
    EXPECT_EQ(lower->head, upper->tail);
    EXPECT_TRUE(m.pairs[0].sourceOnPositiveX);
    EXPECT_EQ(m.pairs[0].source, a.upper);
    EXPECT_EQ(m.pairs[0].mirrored, upper->id);
    EXPECT_TRUE(a.rig.validate());
}

TEST(RigMirrorTest, MirroredRestMatrixIsTheReflectedRestMatrix) {
    ArmRig a = makeArm();
    ASSERT_TRUE(RigMirror::mirrorBones(a.rig, {a.upper}).status);
    const glm::mat4 reflect = glm::mat4(glm::vec4(-1, 0, 0, 0), glm::vec4(0, 1, 0, 0), glm::vec4(0, 0, 1, 0), glm::vec4(0, 0, 0, 1));
    for (const char* name : {"Upper", "Lower", "Hand"}) {
        const glm::mat4 left = AuthoredRig::restMatrix(*a.rig.findByName(std::string(name) + ".L"));
        const glm::mat4 right = AuthoredRig::restMatrix(*a.rig.findByName(std::string(name) + ".R"));
        // The reflection of a rotation frame has a flipped X axis; the bone's own X axis (its roll direction) mirrors to +X of the mirror bone.
        const glm::mat4 reflected = reflect * left * reflect;
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) EXPECT_NEAR(reflected[c][r], right[c][r], 1.0e-5f) << name << " col " << c << " row " << r;
    }
}

TEST(RigMirrorTest, MirrorsBackWhenTheNameCarriesTheRightSuffix) {
    AuthoredRig rig;
    const uint32_t spine = rig.addBone("Spine", {0, 1, 0}, {0, 2, 0}).id;
    const uint32_t right = rig.addBone("Arm.R", {-0.2f, 1.8f, 0}, {-1.0f, 1.8f, 0}, 0.f, spine).id;
    const MirrorResult m = RigMirror::mirrorBones(rig, {right});
    ASSERT_TRUE(m.status) << m.status.error;
    const AuthoredBone* left = rig.findByName("Arm.L");
    ASSERT_TRUE(left);
    EXPECT_FLOAT_EQ(left->head.x, 0.2f);
    EXPECT_FALSE(m.pairs[0].sourceOnPositiveX);
}

TEST(RigMirrorTest, ConnectedBoneOnASharedParentKeepsTheConnectionOnlyWhenItStillMeets) {
    AuthoredRig rig;
    const uint32_t spine = rig.addBone("Spine", {0, 1, 0}, {0, 2, 0}).id;
    // Connected to the spine's tail, which is on the plane: the mirror meets it as well.
    const uint32_t on = rig.addChainBone(spine, "Shoulder.L", {1.0f, 2.0f, 0}).id;
    ASSERT_TRUE(on);
    ASSERT_TRUE(RigMirror::mirrorBones(rig, {on}).status);
    const AuthoredBone* mirrored = rig.findByName("Shoulder.R");
    ASSERT_TRUE(mirrored);
    EXPECT_TRUE(mirrored->connected);
    EXPECT_EQ(mirrored->parent, spine);

    // A parent whose tail is off the plane: the mirrored head cannot reach it, so the copy is unconnected.
    AuthoredRig off;
    const uint32_t beam = off.addBone("Beam", {0.5f, 0, 0}, {0.5f, 1, 0}).id;
    const uint32_t tip = off.addChainBone(beam, "Tip.L", {1.5f, 1, 0}).id;
    ASSERT_TRUE(RigMirror::mirrorBones(off, {tip}).status);
    EXPECT_FALSE(off.findByName("Tip.R")->connected);
    EXPECT_EQ(off.findByName("Tip.R")->parent, beam);
}

TEST(RigMirrorTest, RejectionsLeaveTheRigUntouched) {
    ArmRig a = makeArm();
    const size_t before = a.rig.size();
    const uint64_t revision = a.rig.revision();

    EXPECT_FALSE(RigMirror::mirrorBones(a.rig, {}).status) << "no bone named";
    EXPECT_FALSE(RigMirror::mirrorBones(a.rig, {999}).status) << "unknown bone";
    EXPECT_FALSE(RigMirror::mirrorBones(a.rig, {a.upper, a.upper}).status) << "named twice";
    EXPECT_FALSE(RigMirror::mirrorBones(a.rig, {a.upper, a.lower}).status) << "a root inside another root's hierarchy";
    EXPECT_FALSE(RigMirror::mirrorBones(a.rig, {a.spine}).status) << "centre bone: on the plane and without a side suffix";
    MirrorOptions same;
    same.fromSuffix = same.toSuffix = ".L";
    EXPECT_FALSE(RigMirror::mirrorBones(a.rig, {a.upper}, same).status) << "identical suffixes";

    ASSERT_TRUE(RigMirror::mirrorBones(a.rig, {a.upper}).status);
    const size_t afterFirst = a.rig.size();
    EXPECT_FALSE(RigMirror::mirrorBones(a.rig, {a.upper}).status) << "the mirror names now exist";
    EXPECT_EQ(a.rig.size(), afterFirst);
    EXPECT_EQ(before + 3, afterFirst);
    EXPECT_GT(a.rig.revision(), revision);
}

TEST(RigMirrorTest, BoneOnThePlaneAndBoneWithoutSuffixAreRejected) {
    AuthoredRig rig;
    const uint32_t onPlane = rig.addBone("Tail.L", {0, 0, 0}, {0, 0, 1}).id;
    const MirrorResult a = RigMirror::mirrorBones(rig, {onPlane});
    EXPECT_FALSE(a.status);
    EXPECT_NE(a.status.error.find("X = 0 plane"), std::string::npos) << a.status.error;

    AuthoredRig plain;
    const uint32_t arm = plain.addBone("Arm", {0.5f, 0, 0}, {1.5f, 0, 0}).id;
    const MirrorResult b = RigMirror::mirrorBones(plain, {arm});
    EXPECT_FALSE(b.status);
    EXPECT_NE(b.status.error.find("suffix"), std::string::npos) << b.status.error;
    EXPECT_EQ(plain.size(), 1u);
}

TEST(RigMirrorTest, CustomSuffixes) {
    AuthoredRig rig;
    const uint32_t a = rig.addBone("Left_Arm", {0.5f, 0, 0}, {1.5f, 0, 0}).id;
    MirrorOptions options;
    options.fromSuffix = "_Arm";
    options.toSuffix = "_Wing";
    ASSERT_TRUE(RigMirror::mirrorBones(rig, {a}, options).status);
    EXPECT_TRUE(rig.findByName("Left_Wing"));
    EXPECT_EQ(RigMirror::mirrorName("Hand", MirrorOptions{}), "");
    EXPECT_EQ(RigMirror::mirrorName("Hand.R", MirrorOptions{}), "Hand.L");
}

TEST(HumanoidTemplateTest, BuildsTheBasicBodyWithPairedSides) {
    const HumanoidTemplate t = makeHumanoidTemplate();
    ASSERT_TRUE(t.status) << t.status.error;
    EXPECT_EQ(t.rig.size(), 3u + 6u + 6u);
    for (const char* name : {"Root", "Spine", "Head", "UpperArm.L", "LowerArm.L", "Hand.L", "UpperLeg.L", "LowerLeg.L", "Foot.L",
                             "UpperArm.R", "LowerArm.R", "Hand.R", "UpperLeg.R", "LowerLeg.R", "Foot.R"})
        EXPECT_TRUE(t.rig.findByName(name)) << name;
    ASSERT_EQ(t.leftBones.size(), 6u);
    ASSERT_EQ(t.rightBones.size(), 6u);
    for (uint32_t id : t.leftBones) EXPECT_GT(t.rig.find(id)->head.x + t.rig.find(id)->tail.x, 0.f);
    for (uint32_t id : t.rightBones) EXPECT_LT(t.rig.find(id)->head.x + t.rig.find(id)->tail.x, 0.f);
    EXPECT_EQ(t.rig.findByName("Spine")->parent, t.rig.findByName("Root")->id);
    EXPECT_EQ(t.rig.findByName("UpperArm.R")->parent, t.rig.findByName("Spine")->id);
    EXPECT_EQ(t.rig.findByName("UpperLeg.R")->parent, t.rig.findByName("Root")->id);
    EXPECT_NEAR(t.rig.findByName("Head")->tail.y, 1.7f, 1.0e-5f);
    EXPECT_NEAR(t.rig.findByName("Foot.L")->tail.y, 0.03f, 1.0e-5f);
}

TEST(HumanoidTemplateTest, SizeAndOriginScaleTheWholeBody) {
    const HumanoidTemplate t = makeHumanoidTemplate(3.4f, glm::vec3(10.f, 0.f, -2.f));
    ASSERT_TRUE(t.status) << t.status.error;
    EXPECT_NEAR(t.rig.findByName("Head")->tail.y, 3.4f, 1.0e-4f);
    EXPECT_NEAR(t.rig.findByName("Root")->head.x, 10.f, 1.0e-5f) << "origin shifts the body";
    EXPECT_FALSE(makeHumanoidTemplate(0.f).status);
    EXPECT_FALSE(makeHumanoidTemplate(-1.f).status);
}

TEST(HumanoidTemplateTest, SidesAreMirrorImagesAboutTheOriginPlane) {
    const HumanoidTemplate t = makeHumanoidTemplate();
    ASSERT_TRUE(t.status);
    for (const MirrorPair& pair : t.pairs) {
        const AuthoredBone* l = t.rig.find(pair.source);
        const AuthoredBone* r = t.rig.find(pair.mirrored);
        ASSERT_TRUE(l && r);
        EXPECT_NEAR(l->head.x, -r->head.x, 1.0e-6f);
        EXPECT_NEAR(l->tail.x, -r->tail.x, 1.0e-6f);
        EXPECT_NEAR(l->head.y, r->head.y, 1.0e-6f);
        EXPECT_NEAR(l->tail.z, r->tail.z, 1.0e-6f);
    }
}
