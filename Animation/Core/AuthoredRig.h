#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Skeleton.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Phantom::Animation {

// Result of an editing operation. Every AuthoredRig mutator is atomic: when ok == false the rig is left unchanged.
struct RigResult {
    bool        ok = true;
    std::string error;
    uint32_t    id = 0; // new bone id for the add / duplicate operations

    explicit operator bool() const { return ok; }
    static RigResult success(uint32_t id = 0) { RigResult r; r.id = id; return r; }
    static RigResult failure(std::string message) { RigResult r; r.ok = false; r.error = std::move(message); return r; }
};

// One authored bone, described Blender-style in rig space (the rest pose): head, tail and a roll about the head->tail axis.
// Ids are stable (never reused, start at 1; 0 means "none"), so weights and keys can refer to a bone across edits.
struct AuthoredBone {
    uint32_t    id = 0;
    std::string name;
    uint32_t    parent = 0;       // 0 = root bone
    glm::vec3   head{0.f};
    glm::vec3   tail{0.f, 1.f, 0.f};
    float       roll = 0.f;       // radians
    bool        connected = false; // head follows the parent's tail
};

// Editable skeleton description. Skeleton (the runtime form) is derived from it by toSkeleton().
class AuthoredRig {
public:
    static constexpr size_t MaxBones = 256;       // matches the GPU skin palette
    static constexpr float  MinBoneLength = 1.0e-5f;

    const std::vector<AuthoredBone>& bones() const { return m_bones; }
    size_t size() const { return m_bones.size(); }
    bool empty() const { return m_bones.empty(); }

    // Bumped by every successful edit that changes bone structure or the rest pose. A binding records the value it was
    // computed for, so stale weights can be detected.
    uint64_t revision() const { return m_revision; }

    const AuthoredBone* find(uint32_t id) const;
    const AuthoredBone* findByName(const std::string& name) const;
    std::vector<uint32_t> children(uint32_t id) const;

    // --- Editing -----------------------------------------------------------------------------------------------
    RigResult addBone(const std::string& name, const glm::vec3& head, const glm::vec3& tail, float roll = 0.f,
                      uint32_t parent = 0, bool connected = false);
    // Chain add: head = the parent's tail, connected to it.
    RigResult addChainBone(uint32_t parent, const std::string& name, const glm::vec3& tail, float roll = 0.f);
    // Copies one bone (same parent, same placement). Empty newName -> "<name>.001" style unique name.
    RigResult duplicateBone(uint32_t id, const std::string& newName = {});
    // Children are re-attached to the removed bone's parent (rest placement is unchanged).
    RigResult removeBone(uint32_t id);
    RigResult renameBone(uint32_t id, const std::string& newName);
    // Keeps the rest placement; rejects cycles. A connected bone becomes unconnected unless its head already meets the
    // new parent's tail.
    RigResult setParent(uint32_t id, uint32_t newParent);
    // Rest placement edit. Connected children follow a moved tail; a connected bone cannot have its head moved.
    RigResult setBone(uint32_t id, const glm::vec3& head, const glm::vec3& tail, float roll);

    // Checks every invariant (unique non-empty names, finite values, bone length, parents, no cycles, MaxBones).
    RigResult validate() const;

    // --- Transforms --------------------------------------------------------------------------------------------
    // Rest-pose world (rig space) matrix of a bone: origin at the head, +Y along head->tail, rolled about +Y.
    static glm::mat4 restMatrix(const AuthoredBone& bone);

    // Fills a Skeleton whose bones are ordered parents-first. bindPoseInverse is the inverse rest matrix, local TRS is
    // relative to the parent, so bind-pose skinning is the identity. indexToId (optional) maps skeleton index -> bone id.
    RigResult toSkeleton(Skeleton& out, std::vector<uint32_t>* indexToId = nullptr) const;

private:
    int indexOf(uint32_t id) const;
    bool isDescendant(uint32_t id, uint32_t ancestor) const;
    RigResult commit(AuthoredRig&& candidate);

    std::vector<AuthoredBone> m_bones;
    uint32_t m_nextId = 1;
    uint64_t m_revision = 0;
};

} // namespace Phantom::Animation
