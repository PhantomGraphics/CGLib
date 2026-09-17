#pragma once

#include "NodeId.h"
#include "Transform.h"

#include "json.hpp"

#include <string>
#include <vector>

namespace Phantom::SceneRuntime {

// One component attached to a node. `type` names the component kind the same way
// `.universe`'s entity blocks do (mesh/rigidBody/cloth/fluid keys,
// CGApp/Universe/UniverseSceneIO.cpp) -- `data` carries that kind's fields as an arbitrary
// JSON object rather than a typed C++ struct per kind. This is a deliberate first-slice
// simplification (docs/todo/PLAN_blender_universe_authoring_loop.md Phase 2 item 2):
// versioned typed component schemas, migration, and edit/play snapshots are a separate,
// not-yet-implemented increment layered on top of this generic store.
struct ComponentRecord {
    std::string type;
    nlohmann::json data = nlohmann::json::object();
};

bool operator==(const ComponentRecord& a, const ComponentRecord& b);
inline bool operator!=(const ComponentRecord& a, const ComponentRecord& b) { return !(a == b); }

// One node's own data. SceneGraph owns the parent/children edges separately (mirrors
// AssetManifestEntry / AssetManifest's "value type, container owns the index" split --
// AssetCore/AssetCore/AssetManifest.h) so a node can be looked up, copied, or serialized
// without dragging the whole graph along.
struct SceneNode {
    NodeId id;
    NodeId parent;   // invalid NodeId (AssetId::isValid()==false) == root
    std::string name;
    Transform local;
    bool enabled = true;
    std::vector<ComponentRecord> components;
};

} // namespace Phantom::SceneRuntime
