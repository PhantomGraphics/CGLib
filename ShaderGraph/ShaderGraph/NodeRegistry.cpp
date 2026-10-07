#include "ShaderGraph.h"

#include <cmath>

namespace Phantom::ShaderGraph {

const char* socketTypeName(SocketType t) {
    switch (t) {
        case SocketType::Float: return "Float";
        case SocketType::Vec2: return "Vec2";
        case SocketType::Vec3: return "Vec3";
        case SocketType::Vec4: return "Vec4";
        case SocketType::Color: return "Color";
        case SocketType::Normal: return "Normal";
    }
    return "Float";
}

bool socketTypeFromName(const std::string& s, SocketType& out) {
    for (SocketType t : {SocketType::Float, SocketType::Vec2, SocketType::Vec3, SocketType::Vec4,
                         SocketType::Color, SocketType::Normal}) {
        if (s == socketTypeName(t)) { out = t; return true; }
    }
    return false;
}

int socketComponents(SocketType t) {
    switch (t) {
        case SocketType::Float: return 1;
        case SocketType::Vec2: return 2;
        case SocketType::Vec4: return 4;
        default: return 3;
    }
}

const char* glslTypeName(SocketType t) {
    switch (t) {
        case SocketType::Float: return "float";
        case SocketType::Vec2: return "vec2";
        case SocketType::Vec4: return "vec4";
        default: return "vec3";
    }
}

const Node* Graph::findNode(NodeId id) const {
    for (const Node& n : nodes) if (n.id == id) return &n;
    return nullptr;
}
Node* Graph::findNode(NodeId id) {
    for (Node& n : nodes) if (n.id == id) return &n;
    return nullptr;
}

bool vec4FromJson(const nlohmann::json& j, int components, Vec4f& out) {
    if (j.is_number()) {
        out[0] = j.get<float>();
        return std::isfinite(out[0]);
    }
    if (!j.is_array() || static_cast<int>(j.size()) < components) return false;
    for (int i = 0; i < components && i < 4; ++i) {
        if (!j[i].is_number()) return false;
        out[i] = j[i].get<float>();
        if (!std::isfinite(out[i])) return false;
    }
    return true;
}

namespace {

SocketDef S(const char* name, SocketType t) {
    SocketDef d;
    d.name = name;
    d.type = t;
    return d;
}
SocketDef D(const char* name, SocketType t, Vec4f def) {
    SocketDef d = S(name, t);
    d.def = def;
    d.hasDefault = true;
    return d;
}

const std::vector<std::string> kTypes = {
    "Float", "Color", "Vector", "UV", "ImageTexture", "Add", "Multiply", "Mix", "Clamp",
    "Split", "Combine", "ToColor", "ToVector", "NormalMap", "SurfaceOutput"};

// Data type of the generic math nodes (Add / Multiply / Mix).
bool mathType(const Node& n, SocketType& t, std::string& err) {
    t = SocketType::Float;
    auto it = n.params.find("Type");
    if (it == n.params.end()) return true;
    SocketType parsed;
    if (!it->second.is_string() || !socketTypeFromName(it->second.get<std::string>(), parsed) ||
        parsed == SocketType::Normal || parsed == SocketType::Vec4) {
        err = "Type must be Float, Vec2, Vec3 or Color";
        return false;
    }
    t = parsed;
    return true;
}

}  // namespace

const std::vector<std::string>& registeredNodeTypes() { return kTypes; }

bool isKnownNodeType(const std::string& type) {
    for (const std::string& t : kTypes) if (t == type) return true;
    return false;
}

bool resolveSignature(const Node& n, NodeSignature& o, std::string& err) {
    o = NodeSignature{};
    const std::string& t = n.type;
    SocketType mt;
    if (t == "Float") {
        o.outputs = {S("Value", SocketType::Float)};
        o.valueParams = {D("Value", SocketType::Float, {0, 0, 0, 0})};
    } else if (t == "Color") {
        o.outputs = {S("Color", SocketType::Color)};
        o.valueParams = {D("Color", SocketType::Color, {0.8f, 0.8f, 0.8f, 0})};
    } else if (t == "Vector") {
        o.outputs = {S("Vector", SocketType::Vec3)};
        o.valueParams = {D("Vector", SocketType::Vec3, {0, 0, 0, 0})};
    } else if (t == "UV") {
        o.outputs = {S("UV", SocketType::Vec2)};
    } else if (t == "ImageTexture") {
        o.inputs = {S("UV", SocketType::Vec2)};  // unconnected -> mesh uv
        o.outputs = {S("Color", SocketType::Color), S("Alpha", SocketType::Float)};
    } else if (t == "Add" || t == "Multiply") {
        if (!mathType(n, mt, err)) return false;
        const float id = t == "Add" ? 0.0f : 1.0f;
        o.inputs = {D("A", mt, {id, id, id, 0}), D("B", mt, {id, id, id, 0})};
        o.outputs = {S("Result", mt)};
    } else if (t == "Mix") {
        if (!mathType(n, mt, err)) return false;
        o.inputs = {D("A", mt, {0, 0, 0, 0}), D("B", mt, {1, 1, 1, 0}),
                    D("Factor", SocketType::Float, {0.5f, 0, 0, 0})};
        o.outputs = {S("Result", mt)};
    } else if (t == "Clamp") {
        o.inputs = {D("Value", SocketType::Float, {0, 0, 0, 0}), D("Min", SocketType::Float, {0, 0, 0, 0}),
                    D("Max", SocketType::Float, {1, 0, 0, 0})};
        o.outputs = {S("Result", SocketType::Float)};
    } else if (t == "Split") {
        o.inputs = {D("Vector", SocketType::Vec3, {0, 0, 0, 0})};
        o.outputs = {S("X", SocketType::Float), S("Y", SocketType::Float), S("Z", SocketType::Float)};
    } else if (t == "Combine") {
        o.inputs = {D("X", SocketType::Float, {0, 0, 0, 0}), D("Y", SocketType::Float, {0, 0, 0, 0}),
                    D("Z", SocketType::Float, {0, 0, 0, 0})};
        o.outputs = {S("Vector", SocketType::Vec3)};
    } else if (t == "ToColor") {
        o.inputs = {D("Vector", SocketType::Vec3, {0, 0, 0, 0})};
        o.outputs = {S("Color", SocketType::Color)};
    } else if (t == "ToVector") {
        o.inputs = {D("Color", SocketType::Color, {0, 0, 0, 0})};
        o.outputs = {S("Vector", SocketType::Vec3)};
    } else if (t == "NormalMap") {
        o.inputs = {D("Color", SocketType::Color, {0.5f, 0.5f, 1.0f, 0}),
                    D("Strength", SocketType::Float, {1, 0, 0, 0})};
        o.outputs = {S("Normal", SocketType::Normal)};
    } else if (t == "SurfaceOutput") {
        o.inputs = {D("BaseColor", SocketType::Color, {0.8f, 0.8f, 0.8f, 0}),
                    D("Metallic", SocketType::Float, {0, 0, 0, 0}),
                    D("Roughness", SocketType::Float, {0.5f, 0, 0, 0}),
                    S("Normal", SocketType::Normal)};  // unconnected -> geometric normal
    } else {
        err = "unknown node type '" + t + "'";
        return false;
    }
    return true;
}

}  // namespace Phantom::ShaderGraph
