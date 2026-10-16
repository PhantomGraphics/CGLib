#pragma once

// Re-export merge on a ".universe" v2 document (docs/todo/PLAN_blender_universe_authoring_loop.md
// stage 1). Extracted from CGApp/Universe/SceneMerge.* so Universe (LoadScene) and PhantomStudio
// (`reimport`) run the same code on the same SceneV2 instead of one working on a v1-shaped copy.
//
// When Blender re-exports an asset, every node that came from a sidecar object is matched to the
// new sidecar by UUID and merged three ways:
//
//   base   = what Blender authored when the node was imported (kept in the document as the
//            rigidBody component's "authored" copy and the meshAsset component's
//            "authoredTransform"),
//   local  = the value in the document (adjusted in Studio/Universe or by hand),
//   remote = the value in the re-exported sidecar / GLB.
//
// A field the user changed (local != base) is an override and is kept; every other field takes
// the remote value. Overridable fields: the node's world transform and the rigidBody's physical
// material (mass, restitution, friction, twoWay). The collider shape and size are owned by
// Blender and always follow the re-export. Renames follow the UUID; objects deleted in Blender
// drop their node (its children keep their world transform and become roots); new objects become
// new nodes. Everything is recorded in a MergeReport.

#include "AssetSidecar.h"
#include "SceneV2Editor.h"

#include "json.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Phantom::SceneRuntime {

struct MergeReport {
    std::vector<std::string> renamed;    // "Old -> New"
    std::vector<std::string> removed;    // nodes whose sidecar object is gone
    std::vector<std::string> added;      // new sidecar objects that became nodes
    std::vector<std::string> updated;    // "Node.field": took the re-exported value
    std::vector<std::string> kept;       // "Node.field": local override survived
    std::vector<std::string> conflicts;  // override kept although Blender changed it too, etc.

    bool empty() const;
    // Number of entries in one list ("renamed"/"removed"/"added"/"updated"/"kept"/"conflicts"),
    // or -1 for an unknown key.
    int count(const std::string& key) const;
    nlohmann::json toJson() const;
};

// The overridable physical-material fields of a rigidBody component, by sidecar field name.
extern const char* const kOverridableRigidBodyFields[4];

enum class RigidBodyMergeAction {
    KeepLocal,   // no base to merge against (local-only or pre-merge file): leave the component as is
    UseMerged,   // write `merged` through rigidBodyComponentData()
    Remove,      // Blender removed the component and the document had not changed it
};

struct RigidBodyMerge {
    RigidBodyMergeAction action = RigidBodyMergeAction::KeepLocal;
    SidecarRigidBody merged;
};

// `local`: the node's rigidBody component data (nullptr if it has none). `remote`: the
// re-exported component (nullptr if Blender has none). Fills `report` under `node`'s name.
RigidBodyMerge mergeRigidBody(const std::string& node, const nlohmann::json* local,
                              const SidecarRigidBody* remote, MergeReport& report);

// Transform merge: returns the (world) transform to use. `base` is nullopt for files saved
// before the merge existed (then `local` wins).
Transform mergeTransform(const std::string& node, const Transform& local,
                         const std::optional<Transform>& base, const Transform& remote,
                         MergeReport& report);

bool sameTransform(const Transform& a, const Transform& b);

// {"pos":[3],"rot":[x,y,z,w],"scale":[3]} -- the same shape SceneGraph and "authoredTransform" use.
nlohmann::json transformToJson(const Transform& t);
std::optional<Transform> transformFromJson(const nlohmann::json& j);

// The rigidBody component a sidecar body becomes in the document: world-unit collider sizes
// (scaled by `world.scale`), plane normal/offset, geometryNode for hull/mesh shapes, and the
// untouched Blender component under "authored" (the next merge's base).
nlohmann::json rigidBodyComponentData(const SidecarRigidBody& body, const Transform& world,
                                      const std::string& nodeName);

// Reconciles `doc` with the current GLB and sidecar of every glTF asset it references, using
// per-node "meshAsset" components whose nodeId equals the node UUID. An asset without a sidecar
// is left alone. Malformed input (bad sidecar, missing GLB node, duplicate/empty UUID) returns
// false with `error` set and `doc` unchanged. `scenePath` locates relative asset URIs (with
// doc.assetRoot, as Universe resolves them).
bool mergeReexportedAssets(Universe::SceneV2& doc, const std::filesystem::path& scenePath,
                           MergeReport& report, std::string* error = nullptr);

} // namespace Phantom::SceneRuntime
