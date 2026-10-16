#include <cctype>

#include "ShaderGraph.h"

namespace Phantom::ShaderGraph {

namespace {

bool validName(const std::string& name) {
    if (name.empty()) return false;
    for (unsigned char c : name)
        if (!std::isalnum(c) && c != '_' && c != '-') return false;
    return true;
}

bool inputLinked(const Graph& g, NodeId node, const std::string& socket) {
    for (const Link& l : g.links)
        if (l.toNode == node && l.toSocket == socket) return true;
    return false;
}

nlohmann::json defaultJson(const SocketDef& d) {
    const int comps = socketComponents(d.type);
    if (comps == 1) return d.def[0];
    nlohmann::json a = nlohmann::json::array();
    for (int i = 0; i < comps; ++i) a.push_back(d.def[static_cast<size_t>(i)]);
    return a;
}

// The facts about an exposed parameter, or false if its node / parameter no longer qualifies.
bool resolve(const Graph& g, const ExposedParam& e, ExposedInfo& info) {
    info = ExposedInfo{};
    info.name = e.name;
    info.node = e.node;
    info.param = e.param;
    const Node* n = g.findNode(e.node);
    if (!n) return false;
    if (n->type == "ImageTexture" && e.param == "Path") {
        info.isPath = true;
        const auto it = n->params.find("Path");
        info.value = (it != n->params.end() && it->second.is_string()) ? it->second : nlohmann::json("");
        return true;
    }
    NodeSignature sig;
    std::string err;
    if (!resolveSignature(*n, sig, err)) return false;
    const auto value = n->params.find(e.param);
    for (const SocketDef& d : sig.valueParams) {
        if (d.name != e.param) continue;
        info.type = d.type;
        info.value = value != n->params.end() ? value->second : defaultJson(d);
        return true;
    }
    for (const SocketDef& d : sig.inputs) {
        if (d.name != e.param || !d.hasDefault) continue;  // sockets with a builtin source (uv, normal) hold no value
        info.type = d.type;
        info.value = value != n->params.end() ? value->second : defaultJson(d);
        return !inputLinked(g, e.node, e.param);
    }
    return false;
}

void setError(std::string* error, const std::string& text) {
    if (error) *error = text;
}

}  // namespace

std::vector<ExposedInfo> describeExposed(const Graph& graph) {
    std::vector<ExposedInfo> out;
    for (const ExposedParam& e : graph.exposed) {
        ExposedInfo info;
        info.valid = resolve(graph, e, info);
        out.push_back(std::move(info));
    }
    return out;
}

bool addExposed(Graph& graph, const std::string& name, NodeId node, const std::string& param, std::string* error) {
    if (!validName(name)) {
        setError(error, "exposed name must be non-empty and use letters, digits, underscore or hyphen");
        return false;
    }
    for (const ExposedParam& e : graph.exposed) {
        if (e.name == name) {
            setError(error, "'" + name + "' is already exposed");
            return false;
        }
    }
    const ExposedParam e{name, node, param};
    ExposedInfo info;
    if (!resolve(graph, e, info)) {
        setError(error, "node " + std::to_string(node) + " has no unlinked value '" + param + "' to expose");
        return false;
    }
    graph.exposed.push_back(e);
    return true;
}

bool removeExposed(Graph& graph, const std::string& name) {
    for (auto it = graph.exposed.begin(); it != graph.exposed.end(); ++it) {
        if (it->name == name) {
            graph.exposed.erase(it);
            return true;
        }
    }
    return false;
}

bool applyOverrides(const Graph& asset, const std::map<std::string, nlohmann::json>& overrides, Graph& out,
                    std::string* error) {
    Graph g = asset;
    for (const auto& [name, value] : overrides) {
        const ExposedParam* exposed = nullptr;
        for (const ExposedParam& e : asset.exposed)
            if (e.name == name) exposed = &e;
        if (!exposed) {
            setError(error, "'" + name + "' is not an exposed parameter of the asset");
            return false;
        }
        ExposedInfo info;
        if (!resolve(asset, *exposed, info)) {
            setError(error, "exposed parameter '" + name + "' no longer refers to an unlinked value");
            return false;
        }
        if (info.isPath) {
            if (!value.is_string()) {
                setError(error, "value for '" + name + "' must be a path string");
                return false;
            }
        } else {
            Vec4f parsed{};
            const int comps = socketComponents(info.type);
            if ((comps > 1 && !value.is_array()) || !vec4FromJson(value, comps, parsed)) {
                setError(error, std::string("value for '") + name + "' does not match its type (" + socketTypeName(info.type) + ")");
                return false;
            }
        }
        g.findNode(exposed->node)->params[exposed->param] = value;
    }
    out = std::move(g);
    return true;
}

}  // namespace Phantom::ShaderGraph
