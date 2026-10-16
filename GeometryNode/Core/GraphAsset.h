#pragma once

// Reusable graphs (docs/todo/PLAN_geometry_node.md Phase 6). A graph can name some of its unlinked input sockets
// ("exposed parameters"); a consumer that uses the graph as a shared asset sets those names per use (overrides)
// without touching the graph structure. The effective graph is the asset graph with the overrides written into the
// matching node parameters, so evaluation, caching and JSON need no special case.

#include <map>
#include <string>
#include <vector>

#include "GraphTypes.h"
#include "NodeRegistry.h"

namespace Phantom::GeometryNode {

struct ExposedInfo {
    std::string name;
    NodeId node = 0;
    std::string socket;
    SocketType type = SocketType::Float;
    nlohmann::json value;  // what the asset graph currently holds for it (parameter, else socket default)
    bool valid = false;    // the node, socket and type still exist and the socket is unlinked
};

// Describes every exposed parameter of `graph` (invalid ones are reported with valid == false, never dropped).
std::vector<ExposedInfo> describeExposed(const Graph& graph, const NodeRegistry& registry);

// Exposes input `socket` of `node` as `name`. The socket must be an unlinked, non-geometry, non-field input of a known
// node; names are unique and made of letters, digits, underscore and hyphen. On failure the graph is untouched.
bool addExposed(Graph& graph, const std::string& name, NodeId node, const std::string& socket,
                const NodeRegistry& registry, std::string* error = nullptr);
bool removeExposed(Graph& graph, const std::string& name);

// asset graph + overrides -> effective graph. Every override must name an exposed, valid parameter and carry a value of
// its socket type; otherwise false (with `error`) and `out` is untouched.
bool applyOverrides(const Graph& asset, const std::map<std::string, nlohmann::json>& overrides,
                    const NodeRegistry& registry, Graph& out, std::string* error = nullptr);

}  // namespace Phantom::GeometryNode
