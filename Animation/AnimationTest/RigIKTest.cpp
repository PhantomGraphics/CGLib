#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

#include "../Core/RigIK.h"

using namespace Phantom::Animation;

namespace {

// Spine (0,-1,0)-(0,0,0) with a two bone arm along +X: Upper (0,0,0)-(1,0,0), Lower (1,0,0)-(2,0,0).
struct Arm {
    AuthoredRig rig;
    uint32_t spine = 0, upper = 0, lower = 0;
    Arm() {
        spine = rig.addBone("Spine", {0, -1, 0}, {0, 0, 0}).id;
        upper = rig.addBone("Upper", {0, 0, 0}, {1, 0, 0}, 0.f, spine).id;
        lower = rig.addChainBone(upper, "Lower", {2, 0, 0}).id;
    }
    RigIKRequest request(const glm::vec3& target) const {
        RigIKRequest r;
        r.rig = &rig;
        r.chain = { upper, lower };
        r.parentGlobal = AuthoredRig::restMatrix(*rig.find(spine));
        r.target = target;
        return r;
    }
};

// Effector of the chain when the solver's rotations are applied by hand: parent * restLocal * R, bone by bone.
glm::vec3 effectorOf(const Arm& a, const RigIKResult& s) {
    glm::mat4 parent = AuthoredRig::restMatrix(*a.rig.find(a.spine));
    glm::mat4 prevRest = parent;
    glm::mat4 global = parent;
    const uint32_t ids[2] = { a.upper, a.lower };
    for (int i = 0; i < 2; ++i) {
        const glm::mat4 rest = AuthoredRig::restMatrix(*a.rig.find(ids[i]));
        global = global * glm::inverse(prevRest) * rest * glm::mat4_cast(s.rotation[i]);
        prevRest = rest;
    }
    const AuthoredBone* tip = a.rig.find(a.lower);
    return glm::vec3(global * glm::vec4(0.f, glm::length(tip->tail - tip->head), 0.f, 1.f));
}

} // namespace

TEST(RigIKTest, ReachesAReachableTarget) {
    Arm a;
    const RigIKResult s = RigIK::solve(a.request({ 1.f, 1.f, 0.f }));
    ASSERT_TRUE(s.status) << s.status.error;
    EXPECT_TRUE(s.reached);
    EXPECT_LT(s.distance, 1e-3f);
    const glm::vec3 e = effectorOf(a, s);
    EXPECT_NEAR(e.x, 1.f, 2e-3f);
    EXPECT_NEAR(e.y, 1.f, 2e-3f);
    EXPECT_NEAR(e.z, 0.f, 2e-3f);
}

TEST(RigIKTest, AnUnreachableTargetGivesTheBestFinitePose) {
    Arm a;
    const RigIKResult s = RigIK::solve(a.request({ 0.f, 5.f, 0.f }));
    ASSERT_TRUE(s.status);
    EXPECT_FALSE(s.reached);
    EXPECT_NEAR(s.distance, 3.f, 0.05f) << "the arm stretches straight toward a target 5 away from a shoulder 2 long";
    for (const glm::quat& q : s.rotation) EXPECT_TRUE(std::isfinite(q.w) && std::isfinite(q.x));
    EXPECT_NEAR(effectorOf(a, s).y, 2.f, 0.05f);
}

TEST(RigIKTest, TheSolveDoesNotDependOnAnythingElse) {
    Arm a;
    const RigIKResult x = RigIK::solve(a.request({ 1.f, 1.f, 0.f }));
    const RigIKResult y = RigIK::solve(a.request({ 1.f, 1.f, 0.f }));
    ASSERT_EQ(x.rotation.size(), 2u);
    for (int i = 0; i < 2; ++i) EXPECT_EQ(x.rotation[i], y.rotation[i]);
}

TEST(RigIKTest, JointLimitsHoldAndCostReach) {
    Arm a;
    RigIKRequest r = a.request({ 1.f, 1.f, 0.f });
    IKJointLimit free;
    IKJointLimit locked;
    locked.enabled = true;   // all three axes pinned at 0: the elbow cannot bend
    r.limits = { free, locked };
    const RigIKResult s = RigIK::solve(r);
    ASSERT_TRUE(s.status) << s.status.error;
    const float lowerAngle = 2.f * std::acos(std::min(1.f, std::fabs(s.rotation[1].w)));
    EXPECT_LT(lowerAngle, 0.01f);
    EXPECT_FALSE(s.reached) << "a straight arm cannot put its tip 1.41 from the shoulder and 1 above the axis at once";
    EXPECT_GT(s.distance, 0.1f);

    // A hinge range keeps the elbow inside it.
    IKJointLimit hinge;
    hinge.enabled = true;
    hinge.minRad = { -0.05f, -0.05f, 0.f };
    hinge.maxRad = { 0.05f, 0.05f, 2.4f };
    r.limits = { free, hinge };
    const RigIKResult h = RigIK::solve(r);
    ASSERT_TRUE(h.status);
    EXPECT_TRUE(h.reached);
    EXPECT_GE(h.rotation[1].z, -1e-4f);   // the sign of the bend stays on the allowed side
}

TEST(RigIKTest, RefusesBadChains) {
    Arm a;
    RigIKRequest r = a.request({ 1.f, 1.f, 0.f });
    r.chain = { a.lower, a.upper };
    EXPECT_FALSE(RigIK::solve(r).status);
    r.chain = {};
    EXPECT_FALSE(RigIK::solve(r).status);
    r.chain = { a.upper, 99 };
    EXPECT_FALSE(RigIK::solve(r).status);
    r = a.request({ 1.f, 1.f, 0.f });
    r.target = { NAN, 0.f, 0.f };
    EXPECT_FALSE(RigIK::solve(r).status);
    r = a.request({ 1.f, 1.f, 0.f });
    r.iterations = 0;
    EXPECT_FALSE(RigIK::solve(r).status);
}
