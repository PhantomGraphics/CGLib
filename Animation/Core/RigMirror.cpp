#include "RigMirror.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace Phantom::Animation {

namespace {

bool endsWith(const std::string& s, const std::string& suffix) {
    return !suffix.empty() && s.size() > suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

glm::vec3 reflectX(const glm::vec3& v) { return glm::vec3(-v.x, v.y, v.z); }

void collect(const AuthoredRig& rig, uint32_t id, std::vector<uint32_t>& out, std::unordered_set<uint32_t>& seen) {
    if (!seen.insert(id).second) return;
    out.push_back(id);
    for (uint32_t child : rig.children(id)) collect(rig, child, out, seen);
}

bool isDescendantOf(const AuthoredRig& rig, uint32_t id, uint32_t ancestor) {
    for (const AuthoredBone* b = rig.find(id); b && b->parent != 0; b = rig.find(b->parent))
        if (b->parent == ancestor) return true;
    return false;
}

} // namespace

std::string RigMirror::mirrorName(const std::string& name, const MirrorOptions& options) {
    if (options.fromSuffix.empty() || options.toSuffix.empty() || options.fromSuffix == options.toSuffix) return {};
    if (endsWith(name, options.fromSuffix)) return name.substr(0, name.size() - options.fromSuffix.size()) + options.toSuffix;
    if (endsWith(name, options.toSuffix)) return name.substr(0, name.size() - options.toSuffix.size()) + options.fromSuffix;
    return {};
}

std::vector<uint32_t> RigMirror::mirrorSet(const AuthoredRig& rig, const std::vector<uint32_t>& roots) {
    std::vector<uint32_t> out;
    std::unordered_set<uint32_t> seen;
    for (uint32_t root : roots)
        if (rig.find(root)) collect(rig, root, out, seen);
    return out;
}

MirrorResult RigMirror::mirrorBones(AuthoredRig& rig, const std::vector<uint32_t>& roots, const MirrorOptions& options) {
    MirrorResult result;
    if (roots.empty()) { result.status = RigResult::failure("name at least one bone to mirror"); return result; }
    if (options.fromSuffix.empty() || options.toSuffix.empty() || options.fromSuffix == options.toSuffix) {
        result.status = RigResult::failure("the two side suffixes must be non-empty and different");
        return result;
    }
    for (size_t i = 0; i < roots.size(); ++i) {
        if (!rig.find(roots[i])) { result.status = RigResult::failure("bone does not exist"); return result; }
        for (size_t j = 0; j < roots.size(); ++j) {
            if (i == j) continue;
            if (roots[i] == roots[j] && i < j) { result.status = RigResult::failure("bone '" + rig.find(roots[i])->name + "' is named twice"); return result; }
            if (isDescendantOf(rig, roots[i], roots[j])) {
                result.status = RigResult::failure("bone '" + rig.find(roots[i])->name + "' is already inside the hierarchy of '" + rig.find(roots[j])->name + "'");
                return result;
            }
        }
    }

    const std::vector<uint32_t> set = mirrorSet(rig, roots);
    if (rig.size() + set.size() > AuthoredRig::MaxBones) {
        result.status = RigResult::failure("the mirror would exceed the maximum of " + std::to_string(AuthoredRig::MaxBones) + " bones");
        return result;
    }

    std::unordered_set<std::string> taken;
    for (const AuthoredBone& b : rig.bones()) taken.insert(b.name);
    std::unordered_map<uint32_t, std::string> newNames;
    for (uint32_t id : set) {
        const AuthoredBone& b = *rig.find(id);
        if (std::fabs(b.head.x) <= options.planeTolerance && std::fabs(b.tail.x) <= options.planeTolerance) {
            result.status = RigResult::failure("bone '" + b.name + "' lies on the X = 0 plane, so its mirror would coincide with it");
            return result;
        }
        std::string name = mirrorName(b.name, options);
        if (name.empty()) {
            result.status = RigResult::failure("bone '" + b.name + "' needs a '" + options.fromSuffix + "' or '" + options.toSuffix + "' name suffix to be mirrored");
            return result;
        }
        if (!taken.insert(name).second) {
            result.status = RigResult::failure("a bone named '" + name + "' already exists");
            return result;
        }
        newNames[id] = std::move(name);
    }

    AuthoredRig next = rig;
    std::unordered_map<uint32_t, uint32_t> mirrored;
    for (uint32_t id : set) {
        const AuthoredBone& b = *rig.find(id);
        uint32_t parent = b.parent;
        const bool parentMirrored = parent != 0 && mirrored.count(parent) != 0;
        if (parentMirrored) parent = mirrored[parent];
        const glm::vec3 head = reflectX(b.head), tail = reflectX(b.tail);
        bool connected = false;
        glm::vec3 headUsed = head;
        if (b.connected && parent != 0) {
            const AuthoredBone* p = next.find(parent);
            if (parentMirrored) connected = true;
            else if (p && glm::length(p->tail - head) <= options.planeTolerance) connected = true;
            if (connected && p) headUsed = p->tail;
        }
        RigResult added = next.addBone(newNames[id], headUsed, tail, -b.roll, parent, connected);
        if (!added) { result.status = added; result.pairs.clear(); return result; }
        mirrored[id] = added.id;
        MirrorPair pair;
        pair.source = id;
        pair.mirrored = added.id;
        pair.sourceOnPositiveX = (b.head.x + b.tail.x) > 0.f;
        result.pairs.push_back(pair);
    }
    rig = std::move(next);
    result.status = RigResult::success();
    return result;
}

HumanoidTemplate makeHumanoidTemplate(float height, const glm::vec3& origin) {
    HumanoidTemplate t;
    if (!(std::isfinite(height) && height > 0.01f)) { t.status = RigResult::failure("height must be a positive number"); return t; }
    const float s = height / 1.7f;
    const auto P = [&](float x, float y, float z) { return origin + s * glm::vec3(x, y, z); };

    AuthoredRig rig;
    const auto add = [&](const std::string& name, const glm::vec3& head, const glm::vec3& tail, uint32_t parent, bool connected) {
        return rig.addBone(name, head, tail, 0.f, parent, connected);
    };
    RigResult r = add("Root", P(0, 0.90f, 0), P(0, 1.00f, 0), 0, false);
    if (!r) { t.status = r; return t; }
    const uint32_t root = r.id;
    r = add("Spine", P(0, 1.00f, 0), P(0, 1.35f, 0), root, true);
    if (!r) { t.status = r; return t; }
    const uint32_t spine = r.id;
    r = add("Head", P(0, 1.35f, 0), P(0, 1.70f, 0), spine, true);
    if (!r) { t.status = r; return t; }

    struct Step { const char* name; glm::vec3 tail; };
    const auto chain = [&](uint32_t parent, const char* firstName, const glm::vec3& head, const glm::vec3& firstTail, std::initializer_list<Step> rest) {
        RigResult c = add(firstName, head, firstTail, parent, false);
        if (!c) return c;
        const uint32_t first = c.id;
        uint32_t last = first;
        for (const Step& step : rest) {
            c = rig.addChainBone(last, step.name, step.tail);
            if (!c) return c;
            last = c.id;
        }
        return RigResult::success(first);
    };
    RigResult arm = chain(spine, "UpperArm.L", P(0.12f, 1.30f, 0), P(0.42f, 1.30f, 0), { { "LowerArm.L", P(0.68f, 1.30f, 0) }, { "Hand.L", P(0.82f, 1.30f, 0) } });
    if (!arm) { t.status = arm; return t; }
    RigResult leg = chain(root, "UpperLeg.L", P(0.09f, 0.90f, 0), P(0.09f, 0.50f, 0), { { "LowerLeg.L", P(0.09f, 0.10f, 0) }, { "Foot.L", P(0.09f, 0.03f, 0.18f) } });
    if (!leg) { t.status = leg; return t; }

    MirrorResult m = RigMirror::mirrorBones(rig, { arm.id, leg.id });
    if (!m.status) { t.status = m.status; return t; }
    for (const MirrorPair& pair : m.pairs) {
        t.leftBones.push_back(pair.source);
        t.rightBones.push_back(pair.mirrored);
    }
    t.pairs = std::move(m.pairs);
    t.rig = std::move(rig);
    t.status = t.rig.validate();
    return t;
}

} // namespace Phantom::Animation
