#include "RigIK.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace Phantom::Animation {

namespace {

constexpr size_t kMaxChain = 16;

bool finite(const glm::mat4& m) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(m[c][r])) return false;
    return true;
}

bool finite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

glm::quat rotationOf(const glm::mat4& m) {
    glm::mat3 r(m);
    for (int c = 0; c < 3; ++c) {
        const float l = glm::length(r[c]);
        if (l > 1e-12f) r[c] /= l;
    }
    return glm::normalize(glm::quat_cast(r));
}

} // namespace

namespace RigIK {

RigIKResult solve(const RigIKRequest& request) {
    RigIKResult out;
    auto fail = [&](const std::string& why) { out.status = RigResult::failure(why); return out; };
    if (!request.rig) return fail("no rig");
    const size_t n = request.chain.size();
    if (n == 0) return fail("the chain needs at least one bone");
    if (n > kMaxChain) return fail("the chain has more than " + std::to_string(kMaxChain) + " bones");
    if (!finite(request.target) || !finite(request.parentGlobal)) return fail("the target or the parent transform is not finite");
    if (request.iterations < 1 || request.iterations > 1000) return fail("iterations must be 1..1000");
    if (!(request.stepRad > 0.f) || !std::isfinite(request.stepRad)) return fail("the step limit must be positive");
    if (!request.delta.empty() && request.delta.size() != n) return fail("one pose matrix per chain bone is needed");

    // Chain check: known bones, each the child of the one before, no repeats.
    for (size_t i = 0; i < n; ++i) {
        const AuthoredBone* b = request.rig->find(request.chain[i]);
        if (!b) return fail("the chain names a bone that does not exist");
        if (i > 0 && b->parent != request.chain[i - 1]) return fail("'" + b->name + "' is not a child of '" + request.rig->find(request.chain[i - 1])->name + "'; the chain must be consecutive bones");
        for (size_t k = 0; k < i; ++k)
            if (request.chain[k] == request.chain[i]) return fail("a bone is named twice in the chain");
    }

    // Mini skeleton: 0 = the base (the parent of the chain root, fixed), 1..n = the chain, n + 1 = the effector (the tail of the last bone), n + 2 = the target.
    std::vector<glm::mat4> rest(n);
    for (size_t i = 0; i < n; ++i) rest[i] = AuthoredRig::restMatrix(*request.rig->find(request.chain[i]));
    const AuthoredBone* rootBone = request.rig->find(request.chain[0]);
    glm::mat4 restParent{1.f};
    if (rootBone->parent) restParent = AuthoredRig::restMatrix(*request.rig->find(rootBone->parent));

    Skeleton sk;
    std::vector<glm::mat4> global;
    auto add = [&](int parent, const glm::mat4& restLocal, const glm::mat4& globalMatrix) {
        Bone b;
        b.parentIndex = parent;
        b.localPosition = glm::vec3(restLocal[3]);
        b.localRotation = rotationOf(restLocal);
        sk.addBone(std::move(b));
        global.push_back(globalMatrix);
    };
    {
        Bone base;
        base.name = "base";
        sk.addBone(std::move(base));
        global.push_back(request.parentGlobal);
    }
    std::vector<glm::mat4> restLocal(n);
    for (size_t i = 0; i < n; ++i) {
        restLocal[i] = glm::inverse(i == 0 ? restParent : rest[i - 1]) * rest[i];
        const glm::mat4 d = request.delta.empty() ? glm::mat4{1.f} : request.delta[i];
        if (!finite(d)) return fail("a pose matrix is not finite");
        Bone b;
        b.name = "c" + std::to_string(i);
        b.parentIndex = static_cast<int>(i);
        b.localPosition = glm::vec3(restLocal[i][3]);
        b.localRotation = rotationOf(restLocal[i]);
        sk.addBone(std::move(b));
        global.push_back(global[i] * restLocal[i] * d);
    }
    const float tipLength = glm::length(request.rig->find(request.chain[n - 1])->tail - request.rig->find(request.chain[n - 1])->head);
    {
        Bone effector;
        effector.name = "effector";
        effector.parentIndex = static_cast<int>(n);
        effector.localPosition = glm::vec3(0.f, tipLength, 0.f);
        sk.addBone(std::move(effector));
        global.push_back(global[n] * glm::translate(glm::mat4{1.f}, glm::vec3(0.f, tipLength, 0.f)));
        Bone target;
        target.name = "target";
        sk.addBone(std::move(target));
        global.push_back(glm::translate(glm::mat4{1.f}, request.target));
    }
    if (!finite(global[n + 1])) return fail("the chain is not finite");

    IKChain chain;
    chain.effectorBoneIndex = static_cast<int>(n + 1);
    chain.targetBoneIndex = static_cast<int>(n + 2);
    chain.iterationCount = request.iterations;
    chain.angleLimitRad = request.stepRad;
    for (size_t i = n; i >= 1; --i) {   // child -> parent
        chain.chainBones.push_back(static_cast<int>(i));
        chain.limits.push_back(i - 1 < request.limits.size() ? request.limits[i - 1] : IKJointLimit{});
    }
    IKSolver().solve(sk, { chain }, global);

    out.rotation.resize(n);
    for (size_t i = 0; i < n; ++i) {
        if (!finite(global[i + 1])) return fail("the solve did not give a finite pose");
        const glm::mat4 local = glm::inverse(global[i]) * global[i + 1];
        out.rotation[i] = rotationOf(glm::inverse(restLocal[i]) * local);
    }
    out.effector = glm::vec3(global[n + 1][3]);
    out.distance = glm::length(out.effector - request.target);
    out.reached = out.distance <= request.reachTolerance;
    out.status = RigResult::success();
    return out;
}

} // namespace RigIK

} // namespace Phantom::Animation
