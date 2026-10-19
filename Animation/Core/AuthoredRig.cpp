#include "AuthoredRig.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>

#include <algorithm>
#include <cmath>
#include <functional>

namespace Phantom::Animation {

namespace {

bool isFinite(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool sameLocation(const glm::vec3& a, const glm::vec3& b) {
    return glm::distance(a, b) <= 1.0e-5f * std::max(1.f, std::max(glm::length(a), glm::length(b)));
}

// Rotation taking +Y onto `dir` (unit), well defined for the antiparallel case too.
glm::quat alignYTo(const glm::vec3& dir) {
    const glm::vec3 y(0.f, 1.f, 0.f);
    const float d = glm::dot(y, dir);
    if (d > 0.999999f) return glm::quat(1.f, 0.f, 0.f, 0.f);
    if (d < -0.999999f) return glm::angleAxis(3.14159265358979f, glm::vec3(1.f, 0.f, 0.f));
    const glm::vec3 axis = glm::normalize(glm::cross(y, dir));
    return glm::angleAxis(std::acos(std::clamp(d, -1.f, 1.f)), axis);
}

} // namespace

const AuthoredBone* AuthoredRig::find(uint32_t id) const {
    const int i = indexOf(id);
    return i < 0 ? nullptr : &m_bones[i];
}

const AuthoredBone* AuthoredRig::findByName(const std::string& name) const {
    for (const auto& b : m_bones)
        if (b.name == name) return &b;
    return nullptr;
}

std::vector<uint32_t> AuthoredRig::children(uint32_t id) const {
    std::vector<uint32_t> out;
    for (const auto& b : m_bones)
        if (b.parent == id && id != 0) out.push_back(b.id);
    return out;
}

int AuthoredRig::indexOf(uint32_t id) const {
    if (id == 0) return -1;
    for (size_t i = 0; i < m_bones.size(); ++i)
        if (m_bones[i].id == id) return static_cast<int>(i);
    return -1;
}

bool AuthoredRig::isDescendant(uint32_t id, uint32_t ancestor) const {
    // true if `id` is `ancestor` or lies below it. Bounded walk so a (rejected-elsewhere) cycle cannot hang.
    uint32_t cur = id;
    for (size_t guard = 0; cur != 0 && guard <= m_bones.size(); ++guard) {
        if (cur == ancestor) return true;
        const AuthoredBone* b = find(cur);
        if (!b) return false;
        cur = b->parent;
    }
    return false;
}

RigResult AuthoredRig::commit(AuthoredRig&& candidate) {
    RigResult v = candidate.validate();
    if (!v) return v;
    candidate.m_revision = m_revision + 1;
    *this = std::move(candidate);
    return RigResult::success();
}

RigResult AuthoredRig::validate() const {
    if (m_bones.size() > MaxBones) return RigResult::failure("bone count exceeds " + std::to_string(MaxBones));
    for (size_t i = 0; i < m_bones.size(); ++i) {
        const AuthoredBone& b = m_bones[i];
        if (b.name.empty()) return RigResult::failure("bone name is empty");
        for (size_t j = 0; j < i; ++j) {
            if (m_bones[j].id == b.id) return RigResult::failure("duplicate bone id");
            if (m_bones[j].name == b.name) return RigResult::failure("duplicate bone name: " + b.name);
        }
        if (!isFinite(b.head) || !isFinite(b.tail) || !std::isfinite(b.roll))
            return RigResult::failure("non-finite value in bone: " + b.name);
        if (glm::distance(b.head, b.tail) < MinBoneLength)
            return RigResult::failure("zero-length bone: " + b.name);
        if (b.parent != 0 && !find(b.parent)) return RigResult::failure("missing parent of bone: " + b.name);
        if (b.parent == b.id) return RigResult::failure("bone is its own parent: " + b.name);
        if (b.connected) {
            const AuthoredBone* p = find(b.parent);
            if (!p || !sameLocation(b.head, p->tail))
                return RigResult::failure("connected bone head is not at its parent's tail: " + b.name);
        }
    }
    // Cycle check: following parents from any bone must terminate within size() steps.
    for (const auto& b : m_bones) {
        uint32_t cur = b.parent;
        size_t steps = 0;
        while (cur != 0) {
            if (++steps > m_bones.size()) return RigResult::failure("bone hierarchy has a cycle");
            const AuthoredBone* p = find(cur);
            if (!p) break;
            cur = p->parent;
        }
    }
    return RigResult::success();
}

RigResult AuthoredRig::restore(std::vector<AuthoredBone> bones, uint32_t nextId) {
    AuthoredRig next;
    next.m_bones = std::move(bones);
    for (const auto& b : next.m_bones)
        if (b.id == 0 || b.id >= nextId) return RigResult::failure("bone id is out of range");
    next.m_nextId = std::max<uint32_t>(nextId, 1);
    RigResult v = next.validate();
    if (!v) return v;
    next.m_revision = m_revision + 1;
    *this = std::move(next);
    return RigResult::success();
}

RigResult AuthoredRig::addBone(const std::string& name, const glm::vec3& head, const glm::vec3& tail, float roll,
                               uint32_t parent, bool connected) {
    if (parent != 0 && !find(parent)) return RigResult::failure("parent bone does not exist");
    AuthoredRig next = *this;
    AuthoredBone b;
    b.id = next.m_nextId++;
    b.name = name;
    b.parent = parent;
    b.head = head;
    b.tail = tail;
    b.roll = roll;
    b.connected = connected && parent != 0;
    next.m_bones.push_back(b);
    const uint32_t id = b.id;
    RigResult r = commit(std::move(next));
    if (!r) return r;
    return RigResult::success(id);
}

RigResult AuthoredRig::addChainBone(uint32_t parent, const std::string& name, const glm::vec3& tail, float roll) {
    const AuthoredBone* p = find(parent);
    if (!p) return RigResult::failure("parent bone does not exist");
    return addBone(name, p->tail, tail, roll, parent, true);
}

RigResult AuthoredRig::duplicateBone(uint32_t id, const std::string& newName) {
    const AuthoredBone* src = find(id);
    if (!src) return RigResult::failure("bone does not exist");
    std::string name = newName;
    if (name.empty()) {
        for (int n = 1; n < 100000; ++n) {
            char suffix[16];
            std::snprintf(suffix, sizeof(suffix), ".%03d", n);
            name = src->name + suffix;
            if (!findByName(name)) break;
        }
    }
    // The copy is a free bone at the same place; keeping `connected` is valid since the head still meets the parent tail.
    return addBone(name, src->head, src->tail, src->roll, src->parent, src->connected);
}

RigResult AuthoredRig::removeBone(uint32_t id) {
    const int idx = indexOf(id);
    if (idx < 0) return RigResult::failure("bone does not exist");
    AuthoredRig next = *this;
    const uint32_t newParent = next.m_bones[idx].parent;
    for (auto& b : next.m_bones) {
        if (b.parent != id) continue;
        b.parent = newParent;
        // Keep `connected` only if the head still meets the new parent's tail.
        const AuthoredBone* p = next.find(newParent);
        b.connected = b.connected && p && sameLocation(b.head, p->tail);
    }
    next.m_bones.erase(next.m_bones.begin() + idx);
    return commit(std::move(next));
}

RigResult AuthoredRig::renameBone(uint32_t id, const std::string& newName) {
    const int idx = indexOf(id);
    if (idx < 0) return RigResult::failure("bone does not exist");
    AuthoredRig next = *this;
    next.m_bones[idx].name = newName;
    return commit(std::move(next));
}

RigResult AuthoredRig::setParent(uint32_t id, uint32_t newParent) {
    const int idx = indexOf(id);
    if (idx < 0) return RigResult::failure("bone does not exist");
    if (newParent != 0 && !find(newParent)) return RigResult::failure("parent bone does not exist");
    if (newParent != 0 && isDescendant(newParent, id)) return RigResult::failure("parenting would create a cycle");
    AuthoredRig next = *this;
    AuthoredBone& b = next.m_bones[idx];
    b.parent = newParent;
    const AuthoredBone* p = next.find(newParent);
    b.connected = b.connected && p && sameLocation(b.head, p->tail);
    return commit(std::move(next));
}

RigResult AuthoredRig::setBone(uint32_t id, const glm::vec3& head, const glm::vec3& tail, float roll) {
    const int idx = indexOf(id);
    if (idx < 0) return RigResult::failure("bone does not exist");
    AuthoredRig next = *this;
    AuthoredBone& b = next.m_bones[idx];
    if (b.connected && !sameLocation(head, b.head))
        return RigResult::failure("cannot move the head of a connected bone");
    b.head = head;
    b.tail = tail;
    b.roll = roll;
    // Connected children keep their tails and follow this bone's tail.
    for (auto& c : next.m_bones)
        if (c.parent == id && c.connected) c.head = tail;
    return commit(std::move(next));
}

glm::mat4 AuthoredRig::restMatrix(const AuthoredBone& bone) {
    const glm::vec3 axis = bone.tail - bone.head;
    const float len = glm::length(axis);
    const glm::vec3 dir = len > 0.f ? axis / len : glm::vec3(0.f, 1.f, 0.f);
    glm::quat q = alignYTo(dir) * glm::angleAxis(bone.roll, glm::vec3(0.f, 1.f, 0.f));
    glm::mat4 m = glm::mat4_cast(q);
    m[3] = glm::vec4(bone.head, 1.f);
    return m;
}

RigResult AuthoredRig::toSkeleton(Skeleton& out, std::vector<uint32_t>* indexToId) const {
    RigResult v = validate();
    if (!v) return v;

    // Parents first, siblings in storage order.
    std::vector<uint32_t> order;
    order.reserve(m_bones.size());
    std::function<void(uint32_t)> visit = [&](uint32_t id) {
        order.push_back(id);
        for (const auto& b : m_bones)
            if (b.parent == id) visit(b.id);
    };
    for (const auto& b : m_bones)
        if (b.parent == 0) visit(b.id);

    Skeleton sk;
    std::vector<int> skIndexOfId(m_nextId + 1, -1);
    std::vector<glm::mat4> world(order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        const AuthoredBone& ab = *find(order[i]);
        world[i] = restMatrix(ab);
        // Guard against a transform that cannot be inverted (non-finite after composition).
        const glm::mat4 inv = glm::inverse(world[i]);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(inv[c][r])) return RigResult::failure("bone transform is not invertible: " + ab.name);

        Bone bone;
        bone.name = ab.name;
        bone.parentIndex = ab.parent == 0 ? -1 : skIndexOfId[ab.parent];
        bone.bindPoseInverse = inv;
        glm::mat4 local = world[i];
        if (bone.parentIndex >= 0) local = glm::inverse(world[bone.parentIndex]) * world[i];
        bone.localPosition = glm::vec3(local[3]);
        bone.localRotation = glm::normalize(glm::quat_cast(glm::mat3(local)));
        bone.localScale = glm::vec3(1.f);
        skIndexOfId[ab.id] = sk.addBone(std::move(bone));
    }
    out = std::move(sk);
    if (indexToId) *indexToId = std::move(order);
    return RigResult::success();
}

} // namespace Phantom::Animation
