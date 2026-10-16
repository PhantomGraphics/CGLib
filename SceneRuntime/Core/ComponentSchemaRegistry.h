#pragma once

#include "ComponentSchema.h"
#include "SceneNode.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::SceneRuntime {

// In-memory registry of ComponentSchema keyed by component `type`. A thin, dependency-free
// lookup table -- schemas are registered by whoever owns a component kind (e.g. Universe's
// mesh/rigidBody/cloth/fluid), not discovered here. `toJson()`/`fromJson()` bundle every
// registered schema into one file so Blender and C++ can eventually share it (Phase 5
// item 1); nothing generates or consumes that file yet -- schemas are still registered
// programmatically in this increment.
class ComponentSchemaRegistry {
public:
    // Replaces any existing schema with the same `type`.
    void registerSchema(ComponentSchema schema);
    const ComponentSchema* find(const std::string& type) const;
    std::size_t size() const { return schemas_.size(); }

    // Checks `component.data` against the registered schema for `component.type`.
    std::vector<ComponentValidationIssue> validate(const ComponentRecord& component) const;

    // "phantom.component_schema_registry/1" JSON: every registered schema, each in its own
    // ComponentSchema::toJson() shape. fromJson() rejects the whole file if any one embedded
    // schema fails ComponentSchema::fromJson() -- same reasoning as ComponentSchema::fromJson()
    // itself, a schema is load-bearing, not best-effort data.
    std::string toJson() const;
    static ComponentSchemaRegistry fromJson(const std::string& json, bool* ok = nullptr);

    bool saveToFile(const std::filesystem::path& path) const;
    static ComponentSchemaRegistry loadFromFile(const std::filesystem::path& path, bool* ok = nullptr);

private:
    std::unordered_map<std::string, ComponentSchema> schemas_;
};

} // namespace Phantom::SceneRuntime
