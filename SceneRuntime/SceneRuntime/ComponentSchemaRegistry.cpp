#include "ComponentSchemaRegistry.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Phantom::SceneRuntime {

namespace {

bool matchesType(const nlohmann::json& value, ComponentFieldType type)
{
    switch (type) {
        case ComponentFieldType::Float:
            return value.is_number();
        case ComponentFieldType::Int:
            return value.is_number_integer();
        case ComponentFieldType::Bool:
            return value.is_boolean();
        case ComponentFieldType::String:
            return value.is_string();
        case ComponentFieldType::Vec3:
            return value.is_array() && value.size() == 3
                && std::all_of(value.begin(), value.end(), [](const nlohmann::json& e) { return e.is_number(); });
        case ComponentFieldType::Vec4:
            return value.is_array() && value.size() == 4
                && std::all_of(value.begin(), value.end(), [](const nlohmann::json& e) { return e.is_number(); });
    }
    return false;
}

bool isNumericFieldType(ComponentFieldType type)
{
    return type == ComponentFieldType::Float || type == ComponentFieldType::Int;
}

} // namespace

void ComponentSchemaRegistry::registerSchema(ComponentSchema schema)
{
    schemas_[schema.type] = std::move(schema);
}

const ComponentSchema* ComponentSchemaRegistry::find(const std::string& type) const
{
    auto it = schemas_.find(type);
    return it == schemas_.end() ? nullptr : &it->second;
}

std::vector<ComponentValidationIssue> ComponentSchemaRegistry::validate(const ComponentRecord& component) const
{
    std::vector<ComponentValidationIssue> issues;

    const ComponentSchema* schema = find(component.type);
    if (!schema) {
        issues.push_back({ ComponentValidationIssue::Kind::UnknownComponentType, "",
            "no schema registered for component type '" + component.type + "'" });
        return issues;
    }

    const nlohmann::json& data = component.data;
    for (const ComponentFieldSchema& field : schema->fields) {
        if (!data.is_object() || !data.contains(field.name)) {
            if (field.required) {
                issues.push_back({ ComponentValidationIssue::Kind::MissingRequiredField, field.name,
                    "missing required field '" + field.name + "'" });
            }
            continue;
        }

        const nlohmann::json& value = data[field.name];
        if (!matchesType(value, field.type)) {
            issues.push_back({ ComponentValidationIssue::Kind::WrongType, field.name,
                "field '" + field.name + "' has the wrong type" });
            continue;
        }

        if (isNumericFieldType(field.type) && (field.minValue.has_value() || field.maxValue.has_value())) {
            const double num = value.get<double>();
            if ((field.minValue.has_value() && num < *field.minValue)
                || (field.maxValue.has_value() && num > *field.maxValue)) {
                issues.push_back({ ComponentValidationIssue::Kind::OutOfRange, field.name,
                    "field '" + field.name + "' is out of range" });
            }
        }
    }

    if (data.is_object()) {
        for (auto it = data.begin(); it != data.end(); ++it) {
            const bool known = std::any_of(schema->fields.begin(), schema->fields.end(),
                [&](const ComponentFieldSchema& f) { return f.name == it.key(); });
            if (!known) {
                issues.push_back({ ComponentValidationIssue::Kind::UnknownField, it.key(),
                    "unknown field '" + it.key() + "'" });
            }
        }
    }

    return issues;
}

std::string ComponentSchemaRegistry::toJson() const
{
    nlohmann::json root;
    root["schema"] = "phantom.component_schema_registry/1";
    nlohmann::json schemasJson = nlohmann::json::array();
    for (const auto& entry : schemas_) {
        schemasJson.push_back(nlohmann::json::parse(entry.second.toJson()));
    }
    root["schemas"] = std::move(schemasJson);
    return root.dump();
}

ComponentSchemaRegistry ComponentSchemaRegistry::fromJson(const std::string& json, bool* ok)
{
    if (ok) *ok = false;
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error&) {
        return ComponentSchemaRegistry{};
    }
    if (!root.is_object() || root.value("schema", "") != "phantom.component_schema_registry/1") {
        return ComponentSchemaRegistry{};
    }
    if (!root.contains("schemas") || !root["schemas"].is_array()) return ComponentSchemaRegistry{};

    ComponentSchemaRegistry result;
    for (const auto& sj : root["schemas"]) {
        bool schemaOk = false;
        ComponentSchema schema = ComponentSchema::fromJson(sj.dump(), &schemaOk);
        if (!schemaOk) return ComponentSchemaRegistry{}; // one bad schema rejects the whole file
        result.registerSchema(std::move(schema));
    }

    if (ok) *ok = true;
    return result;
}

bool ComponentSchemaRegistry::saveToFile(const std::filesystem::path& path) const
{
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << toJson();
    return static_cast<bool>(out);
}

ComponentSchemaRegistry ComponentSchemaRegistry::loadFromFile(const std::filesystem::path& path, bool* ok)
{
    if (ok) *ok = false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return ComponentSchemaRegistry{};
    std::ostringstream buf;
    buf << in.rdbuf();
    return fromJson(buf.str(), ok);
}

} // namespace Phantom::SceneRuntime
