#pragma once

#include "ComponentSchema.h"
#include "SceneNode.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::SceneRuntime {

// In-memory registry of ComponentSchema keyed by component `type`. A thin, dependency-free
// lookup table -- schemas are registered by whoever owns a component kind (e.g. Universe's
// mesh/rigidBody/cloth/fluid), not discovered or persisted here. Schema (de)serialization
// (so Blender and C++ can share the same schema file, per Phase 5 item 1) is deferred to a
// later increment, the same way AssetCore's dependency graph and SceneGraph's typed
// migration were deferred from their own first slices.
class ComponentSchemaRegistry {
public:
    // Replaces any existing schema with the same `type`.
    void registerSchema(ComponentSchema schema);
    const ComponentSchema* find(const std::string& type) const;
    std::size_t size() const { return schemas_.size(); }

    // Checks `component.data` against the registered schema for `component.type`.
    std::vector<ComponentValidationIssue> validate(const ComponentRecord& component) const;

private:
    std::unordered_map<std::string, ComponentSchema> schemas_;
};

} // namespace Phantom::SceneRuntime
