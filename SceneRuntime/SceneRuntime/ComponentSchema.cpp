#include "ComponentSchema.h"

#include <fstream>
#include <sstream>

namespace Phantom::SceneRuntime {

namespace {

const char* fieldTypeToString(ComponentFieldType type)
{
    switch (type) {
        case ComponentFieldType::Float: return "float";
        case ComponentFieldType::Int: return "int";
        case ComponentFieldType::Bool: return "bool";
        case ComponentFieldType::String: return "string";
        case ComponentFieldType::Vec3: return "vec3";
        case ComponentFieldType::Vec4: return "vec4";
    }
    return "float";
}

std::optional<ComponentFieldType> fieldTypeFromString(const std::string& s)
{
    if (s == "float") return ComponentFieldType::Float;
    if (s == "int") return ComponentFieldType::Int;
    if (s == "bool") return ComponentFieldType::Bool;
    if (s == "string") return ComponentFieldType::String;
    if (s == "vec3") return ComponentFieldType::Vec3;
    if (s == "vec4") return ComponentFieldType::Vec4;
    return std::nullopt;
}

} // namespace

std::string ComponentSchema::toJson() const
{
    nlohmann::json root;
    root["schema"] = "phantom.component_schema/1";
    root["type"] = type;
    root["version"] = version;

    nlohmann::json fieldsJson = nlohmann::json::array();
    for (const auto& field : fields) {
        nlohmann::json fj;
        fj["name"] = field.name;
        fj["type"] = fieldTypeToString(field.type);
        fj["required"] = field.required;
        if (field.minValue.has_value()) fj["min"] = *field.minValue;
        if (field.maxValue.has_value()) fj["max"] = *field.maxValue;
        if (!field.tooltip.empty()) fj["tooltip"] = field.tooltip;
        fieldsJson.push_back(std::move(fj));
    }
    root["fields"] = std::move(fieldsJson);
    return root.dump();
}

ComponentSchema ComponentSchema::fromJson(const std::string& json, bool* ok)
{
    if (ok) *ok = false;
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error&) {
        return ComponentSchema{};
    }
    if (!root.is_object() || root.value("schema", "") != "phantom.component_schema/1") return ComponentSchema{};
    if (!root.contains("type") || !root["type"].is_string()) return ComponentSchema{};
    if (!root.contains("fields") || !root["fields"].is_array()) return ComponentSchema{};

    ComponentSchema result;
    result.type = root["type"].get<std::string>();
    result.version = root.value("version", 1);

    for (const auto& fj : root["fields"]) {
        if (!fj.is_object() || !fj.contains("name") || !fj["name"].is_string()) return ComponentSchema{};
        if (!fj.contains("type") || !fj["type"].is_string()) return ComponentSchema{};
        const std::optional<ComponentFieldType> fieldType = fieldTypeFromString(fj["type"].get<std::string>());
        if (!fieldType) return ComponentSchema{};

        ComponentFieldSchema field;
        field.name = fj["name"].get<std::string>();
        field.type = *fieldType;
        field.required = fj.value("required", true);
        field.tooltip = fj.value("tooltip", "");
        if (fj.contains("min")) field.minValue = fj["min"].get<double>();
        if (fj.contains("max")) field.maxValue = fj["max"].get<double>();
        result.fields.push_back(std::move(field));
    }

    if (ok) *ok = true;
    return result;
}

bool ComponentSchema::saveToFile(const std::filesystem::path& path) const
{
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << toJson();
    return static_cast<bool>(out);
}

ComponentSchema ComponentSchema::loadFromFile(const std::filesystem::path& path, bool* ok)
{
    if (ok) *ok = false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return ComponentSchema{};
    std::ostringstream buf;
    buf << in.rdbuf();
    return fromJson(buf.str(), ok);
}

} // namespace Phantom::SceneRuntime
