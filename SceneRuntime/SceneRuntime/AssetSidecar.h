#pragma once

#include "ColliderDesc.h"
#include "Transform.h"

#include "json.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::SceneRuntime {

// Blender phantom.asset/1 sidecar (<asset>.phantom.json) reading, shared by Universe and
// PhantomStudio (moved from CGApp/Universe/AssetSidecar.*; pure JSON, no Vulkan/physics).

// Newest rigidBody component schema this build understands (CGApp/blender/phantom_bridge/
// schemas/rigidbody.json "version"). A newer sidecar is rejected with an explicit ERROR
// rather than half-applied. v3 adds the convexHull / mesh shapes.
constexpr int kRigidBodySchemaVersion = 3;

enum class SidecarSeverity { Warn, Error };

struct SidecarRigidBody {
    std::string object;
    int version = 1;
    // v2+: the Blender-resolved collider in the glTF node's local frame, before node scale
    // (v3 only adds the convexHull / mesh shapes, whose geometry comes from the GLB node).
    // v1: shape + a single world-unit "size" (legacy; not scaled with the node).
    ColliderDesc collider;
    bool scaleWithEntity = false;
    float mass = 1.f;
    float restitution = 0.3f;
    float friction = 0.5f;
    bool twoWay = false;
    nlohmann::json raw; // the component exactly as written, so fields we don't read survive a save
};

struct SidecarObject {
    std::string name;
    std::optional<SidecarRigidBody> rigidBody;
    std::string uuid;
};

// <asset>.phantom.json next to `assetPath`.
std::filesystem::path sidecarPathFor(const std::filesystem::path& assetPath);

// Parses one sidecar rigidBody component. On failure fills `diagnostic` (and `severity` --
// Error for a schema version this build does not know, Warn for malformed data).
bool parseSidecarRigidBody(const nlohmann::json& body, SidecarRigidBody& parsed,
                           std::string& diagnostic, SidecarSeverity& severity);

// Reads the per-object entries of a sidecar. Any invalid component rejects the whole sidecar;
// a missing file is normal (returns true with no objects).
bool readSidecarObjects(const std::filesystem::path& path, std::vector<SidecarObject>& result,
                        std::string& diagnostic, SidecarSeverity* severityOut = nullptr);

// World transform of every named node of a .glb/.gltf (first node of a name wins; TRS or
// matrix, composed through the parent chain). Only the JSON part is read, no mesh data.
// False if the file is not a parsable glTF.
bool readGltfNodeWorldTransforms(const std::filesystem::path& path,
                                 std::unordered_map<std::string, Transform>& out,
                                 std::string& diagnostic);

} // namespace Phantom::SceneRuntime
