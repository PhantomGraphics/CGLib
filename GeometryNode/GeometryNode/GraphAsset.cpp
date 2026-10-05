#include "GraphAsset.h"

#include <cctype>

namespace Phantom::GeometryNode {

namespace {

bool validName(const std::string& name) {
    if (name.empty()) return false;
    for (unsigned char c : name)
        if (!std::isalnum(c) && c != '_' && c != '-') return false;
    return true;
}

bool socketLinked(const Graph& g, NodeId node, const std::string& socket) {
    for (const Link& l : g.links)
        if (l.to.node == node && l.to.socket == socket) return true;
    return false;
}

// The exposed-socket facts, or false if the node / definition / socket no longer qualifies.
bool resolve(const Graph& g, const ExposedParam& e, const NodeRegistry& registry, SocketType& type, nlohmann::json& value) {
    const Node* n = g.findNode(e.node);
    if (!n) return false;
    const NodeDefinition* def = registry.find(n->type);
    const SocketDef* s = def ? def->findInput(e.socket) : nullptr;
    if (!s || s->type == SocketType::Geometry || isFieldType(s->type) || s->multi) return false;
    type = s->type;
    const auto it = n->params.find(e.socket);
    if (it != n->params.end()) {
        value = it->second;
    } else {
        value = nlohmann::json();
        paramToJson(s->defaultValue, value);
    }
    return !socketLinked(g, e.node, e.socket);
}

void setError(std::string* error, const std::string& text) {
    if (error) *error = text;
}

}  // namespace

std::vector<ExposedInfo> describeExposed(const Graph& graph, const NodeRegistry& registry) {
    std::vector<ExposedInfo> out;
    for (const ExposedParam& e : graph.exposed) {
        ExposedInfo info;
        info.name = e.name;
        info.node = e.node;
        info.socket = e.socket;
        info.valid = resolve(graph, e, registry, info.type, info.value);
        out.push_back(std::move(info));
    }
    return out;
}

bool addExposed(Graph& graph, const std::string& name, NodeId node, const std::string& socket,
                const NodeRegistry& registry, std::string* error) {
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
    ExposedParam e{name, node, socket};
    SocketType type;
    nlohmann::json value;
    if (!resolve(graph, e, registry, type, value)) {
        setError(error, "node " + std::to_string(node) + " has no unlinked single-value input '" + socket + "' to expose");
        return false;
    }
    graph.exposed.push_back(std::move(e));
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

bool applyOverrides(const Graph& asset, const std::map<std::string, nlohmann::json>& overrides,
                    const NodeRegistry& registry, Graph& out, std::string* error) {
    Graph g = asset;
    for (const auto& [name, value] : overrides) {
        const ExposedParam* exposed = nullptr;
        for (const ExposedParam& e : asset.exposed)
            if (e.name == name) exposed = &e;
        if (!exposed) {
            setError(error, "'" + name + "' is not an exposed parameter of the asset");
            return false;
        }
        SocketType type;
        nlohmann::json current;
        if (!resolve(asset, *exposed, registry, type, current)) {
            setError(error, "exposed parameter '" + name + "' no longer refers to an unlinked single-value input");
            return false;
        }
        Value parsed;
        if (!paramFromJson(type, value, parsed)) {
            setError(error, "value for '" + name + "' does not match its type (" + toString(type) + ")");
            return false;
        }
        g.findNode(exposed->node)->params[exposed->socket] = value;
    }
    out = std::move(g);
    return true;
}

}  // namespace Phantom::GeometryNode
