#include "PhmatGraph.h"

#include "json.hpp"

#include <queue>
#include <unordered_map>

namespace Phantom::Gltf::Phmat
{
namespace {

using Json = nlohmann::json;

bool parseConstantValue(const Json& j, ValueType& outType, glm::vec4& outValue)
{
    if (j.is_number()) {
        outType = ValueType::Float;
        outValue = glm::vec4(j.get<float>(), 0.f, 0.f, 0.f);
        return true;
    }
    if (j.is_array()) {
        const size_t n = j.size();
        if (n < 2 || n > 4) return false;
        glm::vec4 v(0.f);
        for (size_t i = 0; i < n; ++i) {
            if (!j[i].is_number()) return false;
            v[static_cast<int>(i)] = j[i].get<float>();
        }
        outValue = v;
        outType = (n == 2) ? ValueType::Vec2 : (n == 3 ? ValueType::Vec3 : ValueType::Vec4);
        return true;
    }
    return false;
}

bool parseTextureSlot(const std::string& s, TextureSlot& out)
{
    if (s == "baseColor")          { out = TextureSlot::BaseColor;          return true; }
    if (s == "metallicRoughness")  { out = TextureSlot::MetallicRoughness;  return true; }
    if (s == "normal")             { out = TextureSlot::Normal;             return true; }
    if (s == "occlusion")          { out = TextureSlot::Occlusion;          return true; }
    if (s == "emissive")           { out = TextureSlot::Emissive;           return true; }
    return false;
}

bool parseMathOp(const std::string& s, MathOp& out)
{
    if (s == "add")      { out = MathOp::Add;      return true; }
    if (s == "subtract") { out = MathOp::Subtract; return true; }
    if (s == "multiply") { out = MathOp::Multiply; return true; }
    if (s == "divide")   { out = MathOp::Divide;   return true; }
    if (s == "min")      { out = MathOp::Min;      return true; }
    if (s == "max")      { out = MathOp::Max;      return true; }
    return false;
}

void addError(std::vector<PhmatDiagnostic>& diags, const std::string& nodeId, std::string msg)
{
    diags.push_back(PhmatDiagnostic{ PhmatDiagnostic::Severity::Error, nodeId, std::move(msg) });
}

// This node's input references (dependency edges). Order is not significant.
std::vector<std::string> inputsOf(const PhmatNode& n)
{
    std::vector<std::string> refs;
    switch (n.type) {
    case NodeType::Constant:
    case NodeType::TextureSlot:
    case NodeType::Uv:
        break;
    case NodeType::NormalMap:
        refs = { n.normalMapScale };
        break;
    case NodeType::Mix:
        refs = { n.mixA, n.mixB, n.mixFactor };
        break;
    case NodeType::Math:
        refs = { n.mathA, n.mathB };
        break;
    case NodeType::PbrOutput:
        refs = { n.pbrBaseColor, n.pbrMetallic, n.pbrRoughness };
        for (const std::string& opt : { n.pbrNormal, n.pbrOcclusion, n.pbrEmissive, n.pbrAlpha })
            if (!opt.empty()) refs.push_back(opt);
        break;
    }
    return refs;
}

} // namespace

bool parsePhmatGraph(const std::string& jsonText, PhmatGraph& outGraph,
                      std::vector<PhmatDiagnostic>& outDiagnostics)
{
    outGraph = PhmatGraph{};

    Json root = Json::parse(jsonText, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded() || !root.is_object()) {
        addError(outDiagnostics, "", "malformed JSON document");
        return false;
    }

    outGraph.version = root.value("version", 0);
    if (outGraph.version != 1) {
        addError(outDiagnostics, "", "unsupported .phmat version (only version 1 is recognized)");
        return false;
    }

    outGraph.outputNode = root.value("output", "");
    if (outGraph.outputNode.empty()) {
        addError(outDiagnostics, "", "missing top-level \"output\" (id of the terminal pbrOutput node)");
        return false;
    }

    if (!root.contains("nodes") || !root["nodes"].is_array()) {
        addError(outDiagnostics, "", "missing or non-array top-level \"nodes\"");
        return false;
    }

    bool ok = true;
    for (const Json& j : root["nodes"]) {
        if (!j.is_object() || !j.contains("id") || !j["id"].is_string() ||
            !j.contains("type") || !j["type"].is_string()) {
            addError(outDiagnostics, "", "each node needs a string \"id\" and \"type\"");
            ok = false;
            continue;
        }
        PhmatNode node;
        node.id = j.value("id", "");
        const std::string typeStr = j.value("type", "");

        if (typeStr == "constant") {
            node.type = NodeType::Constant;
            if (!j.contains("value") || !parseConstantValue(j["value"], node.constantType, node.constantValue)) {
                addError(outDiagnostics, node.id, "constant node needs \"value\" as a number or a 2-4 element array");
                ok = false;
            }
        } else if (typeStr == "textureSlot") {
            node.type = NodeType::TextureSlot;
            if (!j.contains("slot") || !j["slot"].is_string() || !parseTextureSlot(j.value("slot", ""), node.textureSlot)) {
                addError(outDiagnostics, node.id, "textureSlot node needs a \"slot\" of baseColor/metallicRoughness/normal/occlusion/emissive");
                ok = false;
            }
        } else if (typeStr == "uv") {
            node.type = NodeType::Uv;
            node.uvSet = j.value("set", 0);
        } else if (typeStr == "normalMap") {
            node.type = NodeType::NormalMap;
            node.normalMapScale = j.value("scale", "");
            if (node.normalMapScale.empty()) {
                addError(outDiagnostics, node.id, "normalMap node needs a \"scale\" node id reference");
                ok = false;
            }
        } else if (typeStr == "mix") {
            node.type = NodeType::Mix;
            node.mixA = j.value("a", "");
            node.mixB = j.value("b", "");
            node.mixFactor = j.value("factor", "");
            if (node.mixA.empty() || node.mixB.empty() || node.mixFactor.empty()) {
                addError(outDiagnostics, node.id, "mix node needs \"a\", \"b\", and \"factor\" node id references");
                ok = false;
            }
        } else if (typeStr == "math") {
            node.type = NodeType::Math;
            node.mathA = j.value("a", "");
            node.mathB = j.value("b", "");
            if (node.mathA.empty() || node.mathB.empty() ||
                !j.contains("op") || !j["op"].is_string() || !parseMathOp(j.value("op", ""), node.mathOp)) {
                addError(outDiagnostics, node.id, "math node needs \"a\", \"b\" node id references and an \"op\" of add/subtract/multiply/divide/min/max");
                ok = false;
            }
        } else if (typeStr == "pbrOutput") {
            node.type = NodeType::PbrOutput;
            node.pbrBaseColor = j.value("baseColor", "");
            node.pbrMetallic  = j.value("metallic", "");
            node.pbrRoughness = j.value("roughness", "");
            node.pbrNormal    = j.value("normal", "");
            node.pbrOcclusion = j.value("occlusion", "");
            node.pbrEmissive  = j.value("emissive", "");
            node.pbrAlpha     = j.value("alpha", "");
            if (node.pbrBaseColor.empty() || node.pbrMetallic.empty() || node.pbrRoughness.empty()) {
                addError(outDiagnostics, node.id, "pbrOutput node needs \"baseColor\", \"metallic\", and \"roughness\" node id references");
                ok = false;
            }
        } else {
            addError(outDiagnostics, node.id, "unsupported node type: " + typeStr);
            ok = false;
            continue; // do not add a half-formed node
        }

        outGraph.nodes.push_back(std::move(node));
    }

    return ok;
}

bool validateAndSort(const PhmatGraph& graph, std::vector<std::string>& outTopoOrder,
                      std::vector<PhmatDiagnostic>& outDiagnostics)
{
    outTopoOrder.clear();
    bool ok = true;

    std::unordered_map<std::string, const PhmatNode*> byId;
    for (const auto& n : graph.nodes) {
        if (byId.count(n.id)) {
            addError(outDiagnostics, n.id, "duplicate node id");
            ok = false;
            continue;
        }
        byId[n.id] = &n;
    }

    if (graph.outputNode.empty() || !byId.count(graph.outputNode)) {
        addError(outDiagnostics, graph.outputNode, "\"output\" does not reference an existing node");
        return false;
    }
    if (byId[graph.outputNode]->type != NodeType::PbrOutput) {
        addError(outDiagnostics, graph.outputNode, "\"output\" must reference a pbrOutput node");
        ok = false;
    }

    for (const auto& n : graph.nodes) {
        for (const auto& ref : inputsOf(n)) {
            if (!byId.count(ref)) {
                addError(outDiagnostics, n.id, "unknown input reference: " + ref);
                ok = false;
            }
        }
    }
    if (!ok) return false; // further checks assume every reference resolves to a real node

    for (const auto& n : graph.nodes) {
        if (n.type == NodeType::Uv && n.uvSet != 0) {
            addError(outDiagnostics, n.id, "uv node: only set=0 is supported in this version");
            ok = false;
        }
    }

    // Kahn's algorithm -- also serves as the cycle check.
    std::unordered_map<std::string, int> indegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents;
    for (const auto& n : graph.nodes) indegree[n.id] = 0;
    for (const auto& n : graph.nodes) {
        for (const auto& ref : inputsOf(n)) {
            dependents[ref].push_back(n.id);
            indegree[n.id]++;
        }
    }

    std::queue<std::string> ready;
    for (const auto& n : graph.nodes) if (indegree[n.id] == 0) ready.push(n.id);

    std::vector<std::string> order;
    order.reserve(graph.nodes.size());
    while (!ready.empty()) {
        std::string id = ready.front();
        ready.pop();
        order.push_back(id);
        for (const auto& dep : dependents[id]) {
            if (--indegree[dep] == 0) ready.push(dep);
        }
    }

    if (order.size() != graph.nodes.size()) {
        addError(outDiagnostics, "", "cycle detected in node graph");
        return false;
    }
    if (!ok) return false;

    // Type checking, in topological order -- every reference has already been confirmed to
    // resolve to a real node above, so typeOf() never misses.
    std::unordered_map<std::string, ValueType> types;
    auto typeOf = [&](const std::string& id) { return types.at(id); };

    for (const auto& id : order) {
        const PhmatNode& n = *byId[id];
        switch (n.type) {
        case NodeType::Constant:
            types[id] = n.constantType;
            break;
        case NodeType::TextureSlot:
            types[id] = ValueType::Vec4;
            break;
        case NodeType::Uv:
            types[id] = ValueType::Vec2;
            break;
        case NodeType::NormalMap:
            if (typeOf(n.normalMapScale) != ValueType::Float) {
                addError(outDiagnostics, id, "normalMap \"scale\" must be a Float node");
                ok = false;
            }
            types[id] = ValueType::Vec3;
            break;
        case NodeType::Mix:
            if (typeOf(n.mixA) != typeOf(n.mixB)) {
                addError(outDiagnostics, id, "mix \"a\" and \"b\" must share the same type");
                ok = false;
            }
            if (typeOf(n.mixFactor) != ValueType::Float) {
                addError(outDiagnostics, id, "mix \"factor\" must be a Float node");
                ok = false;
            }
            types[id] = typeOf(n.mixA);
            break;
        case NodeType::Math:
            if (typeOf(n.mathA) != typeOf(n.mathB)) {
                addError(outDiagnostics, id, "math \"a\" and \"b\" must share the same type");
                ok = false;
            }
            types[id] = typeOf(n.mathA);
            break;
        case NodeType::PbrOutput: {
            auto expect = [&](const std::string& ref, ValueType want, const char* field) {
                if (!ref.empty() && typeOf(ref) != want) {
                    addError(outDiagnostics, id, std::string("pbrOutput \"") + field + "\" has the wrong type");
                    ok = false;
                }
            };
            expect(n.pbrBaseColor, ValueType::Vec4,  "baseColor");
            expect(n.pbrMetallic,  ValueType::Float, "metallic");
            expect(n.pbrRoughness, ValueType::Float, "roughness");
            expect(n.pbrNormal,    ValueType::Vec3,  "normal");
            expect(n.pbrOcclusion, ValueType::Float, "occlusion");
            expect(n.pbrEmissive,  ValueType::Vec3,  "emissive");
            expect(n.pbrAlpha,     ValueType::Float, "alpha");
            types[id] = ValueType::Vec4; // nominal -- nothing downstream consumes a pbrOutput
            break;
        }
        }
    }

    if (!ok) return false;
    outTopoOrder = std::move(order);
    return true;
}

}
