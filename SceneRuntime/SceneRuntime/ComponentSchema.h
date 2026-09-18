#pragma once

#include "json.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Phantom::SceneRuntime {

enum class ComponentFieldType {
    Float,
    Int,
    Bool,
    String,
    Vec3,
    Vec4,
};

struct ComponentFieldSchema {
    std::string name;
    ComponentFieldType type = ComponentFieldType::Float;
    std::optional<double> minValue; // only meaningful for Float/Int
    std::optional<double> maxValue;
    std::string tooltip;
    bool required = true;
    // A default value a UI builder (e.g. the Blender panel Phase 5 item 1 generates) can seed a
    // new component's field with -- shaped to match `type` (a JSON number for Float/Int, bool
    // for Bool, string for String, a 3/4-element array for Vec3/Vec4). Null (the default) means
    // "no default declared". Not validated against `type` here -- ComponentSchemaRegistry::
    // validate() checks actual component data, not the schema's own declared defaults. Appended
    // at the end (like GlobalUBO's exposure field, CameraUBO.h) so every existing positional
    // aggregate-init call site (`ComponentFieldSchema{name, type, min, max, tooltip, required}`,
    // e.g. in ComponentSchemaTest.cpp/ComponentSchemaRegistryTest.cpp) keeps compiling unchanged.
    nlohmann::json defaultValue = nullptr;
};

// A single component kind's versioned field schema -- the "typed component schema" half of
// Blender->Universe authoring loop Phase 2 item 2, and the shape Phase 5 item 1 wants the
// Blender panel to eventually generate its UI from ("UI定義をPythonに手書きで複製せず、
// C++側と共有するversion付きJSON schemaからproperty、範囲、default、tooltipを生成する").
// SceneGraph itself never enforces this at addNode() time -- components stay opaque JSON
// there (Phase 2 item 2's first-slice decision, docs/spec/phantom_scene_runtime.md).
// Validation against a schema is opt-in via ComponentSchemaRegistry::validate(), for
// tooling that wants to flag mistakes rather than silently accept or drop them.
struct ComponentSchema {
    std::string type;
    int version = 1;
    std::vector<ComponentFieldSchema> fields;

    // Single-line "phantom.component_schema/1" JSON -- the (de)serialization Phase 5 item 1
    // needs so a schema authored once (in C++ or, later, shared with the Blender panel) can
    // travel as a file rather than only existing as code calling registerSchema(). Strict:
    // an unrecognized field `type` string or a missing name/type rejects the whole schema
    // (fromJson() returns a default-constructed ComponentSchema, ok=false) rather than
    // silently dropping the one bad field -- a schema is a small, load-bearing contract, not
    // best-effort scene data.
    std::string toJson() const;
    static ComponentSchema fromJson(const std::string& json, bool* ok = nullptr);

    bool saveToFile(const std::filesystem::path& path) const;
    static ComponentSchema loadFromFile(const std::filesystem::path& path, bool* ok = nullptr);
};

struct ComponentValidationIssue {
    enum class Kind {
        UnknownComponentType, // no schema registered for this component's `type`
        MissingRequiredField,
        WrongType,
        OutOfRange,
        UnknownField, // present in the data but not declared by the schema -- flagged, never dropped
    };
    Kind kind;
    std::string field; // empty for UnknownComponentType
    std::string message;
};

} // namespace Phantom::SceneRuntime
