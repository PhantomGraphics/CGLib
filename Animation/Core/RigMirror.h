#pragma once

#include "AuthoredRig.h"

#include <string>
#include <utility>
#include <vector>

namespace Phantom::Animation {

// Mirroring of an authored bone hierarchy across the rig-local X = 0 plane, and the basic humanoid template built from it.
// Both work on an unbound AuthoredRig only (the caller checks), and both are atomic: when the result is not ok the rig is unchanged.

struct MirrorOptions {
    // Side names. A bone whose name ends with `fromSuffix` receives the name with `toSuffix` instead ("UpperArm.L" -> "UpperArm.R");
    // a bone whose name ends with `toSuffix` is mirrored the other way. Names that end with neither are rejected, because the mirror
    // would have no name that tells the two sides apart.
    std::string fromSuffix = ".L";
    std::string toSuffix = ".R";
    float planeTolerance = 1.0e-4f; // |x| at or below this counts as being on the X = 0 plane
};

struct MirrorPair {
    uint32_t source = 0;
    uint32_t mirrored = 0;
    bool sourceOnPositiveX = true; // mean X of the source bone (head and tail) is on the +X side
};

struct MirrorResult {
    RigResult status;
    std::vector<MirrorPair> pairs; // in creation order (parents first)
};

namespace RigMirror {

// The mirror name of `name` under `options`; empty when the name carries neither suffix.
std::string mirrorName(const std::string& name, const MirrorOptions& options);

// Mirrors each given bone together with all of its descendants. A mirrored bone's parent is the mirror of its parent when that parent is
// also mirrored, otherwise the same parent (a shared centre bone). A mirrored bone stays connected when its mirrored head still meets its
// parent's tail. Roll flips sign (an X reflection reverses rotation about the bone axis). Rejected before anything changes:
// an unknown or duplicated root, a root that is a descendant of another root, a bone lying on the X = 0 plane (it would coincide with
// its own mirror), a name without a side suffix, a name that already exists, more bones than the rig can hold.
MirrorResult mirrorBones(AuthoredRig& rig, const std::vector<uint32_t>& roots, const MirrorOptions& options = {});

// The bones mirrorBones would copy for `roots` (roots plus descendants, in rig order), without changing anything.
std::vector<uint32_t> mirrorSet(const AuthoredRig& rig, const std::vector<uint32_t>& roots);

} // namespace RigMirror

// The basic humanoid: Root, Spine, Head, and per side UpperArm / LowerArm / Hand and UpperLeg / LowerLeg / Foot. No fingers or face.
// Built once for the left (+X) side with the ".L" suffix and mirrored for ".R". `height` is the standing height in rig units (feet at y = 0,
// facing +Z); `origin` shifts every bone.
struct HumanoidTemplate {
    RigResult status;
    AuthoredRig rig;
    std::vector<uint32_t> leftBones;  // ".L" bone ids
    std::vector<uint32_t> rightBones; // ".R" bone ids
    std::vector<MirrorPair> pairs;    // left -> right
};

HumanoidTemplate makeHumanoidTemplate(float height = 1.7f, const glm::vec3& origin = glm::vec3(0.f));

} // namespace Phantom::Animation
