#pragma once

#include "UniverseSceneV2.h"

#include "json.hpp"

#include <optional>
#include <string>
#include <vector>

namespace Phantom::SceneRuntime::Universe {

// UI-independent editing API over a ".universe" v2 document (Blender->Universe authoring loop
// stage 1, docs/todo/PLAN_blender_universe_authoring_loop.md). Studio's command session calls
// this instead of flattening the asset-backed scene into its own primitive model, so a
// "meshAsset" component's assetId/nodeId, every node UUID, the rigidBody "authored" base copy,
// "authoredTransform" and any field this API does not know all survive an edit + save untouched.
//
// Edits only ever touch the local value (node.local, the rigidBody's own mass/friction/...);
// the authored base stays as Blender exported it, so Universe's re-export three-way merge sees
// the edit as a local override. No function throws; failures come back in EditResult and leave
// the document unchanged.
struct EditResult {
    bool ok = true;
    std::string code;    // "not_found", "ambiguous", "out_of_range", "no_component", "unknown_field"
    std::string message;

    static EditResult success() { return {}; }
    static EditResult failure(std::string c, std::string m) { return { false, std::move(c), std::move(m) }; }
};

// Resolves `ref` (a node UUID, else a unique node name) to a node. nullptr when absent or when
// several nodes share the name (`error` says which).
const SceneNode* findNodeByRef(const SceneV2& doc, const std::string& ref, EditResult* error = nullptr);
SceneNode* findNodeByRef(SceneV2& doc, const std::string& ref, EditResult* error = nullptr);

// Depth-first, siblings sorted by name then id, so the order is stable across runs.
std::vector<const SceneNode*> listNodes(const SceneV2& doc);

// {"id","name","parent","enabled","translation":[3],"rotationEulerDeg":[3],"scale":[3],
//  "components":[type...],"meshAsset":{assetId,nodeId,uri}?,"rigidBody":{...}?}
nlohmann::json describeNode(const SceneV2& doc, const SceneNode& node);
nlohmann::json describeDocument(const SceneV2& doc);

// Any of the three may be absent. rotation is Euler degrees (glm::eulerAngles convention).
// Non-finite values and a zero scale component are rejected.
struct TransformEdit {
    std::optional<Phantom::Math::Vector3df> translation;
    std::optional<Phantom::Math::Vector3df> rotationEulerDeg;
    std::optional<Phantom::Math::Vector3df> scale;
};
EditResult setNodeTransform(SceneV2& doc, const std::string& ref, const TransformEdit& edit);

// Fields: "mass" (>=0), "friction" (>=0), "restitution" (0..1), "twoWay" (0/1/true/false).
// The node must already have a rigidBody component (adding one needs a collider description).
EditResult setRigidBodyField(SceneV2& doc, const std::string& ref, const std::string& field,
                             const std::string& value);

} // namespace Phantom::SceneRuntime::Universe
