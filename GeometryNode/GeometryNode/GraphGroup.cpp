#include "GraphGroup.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <set>

#include "GraphEvaluator.h"
#include "GraphJson.h"

namespace Phantom::GeometryNode {

namespace {

using GroupLibrary = std::vector<GroupDef>;
using LibraryPtr = std::shared_ptr<const GroupLibrary>;

void setError(std::string* error, const std::string& text) {
    if (error) *error = text;
}

bool validGroupName(const std::string& name) {
    if (name.empty()) return false;
    for (unsigned char c : name)
        if (!std::isalnum(c) && c != '_' && c != '-') return false;
    return true;
}

bool isValueType(SocketType t) { return t != SocketType::Geometry && !isFieldType(t); }

Value zeroValue(SocketType t) {
    switch (t) {
        case SocketType::Float: return 0.0f;
        case SocketType::Int: return int32_t(0);
        case SocketType::Bool: return false;
        case SocketType::Vector3: return Vec3{};
        default: return {};
    }
}

SocketDef socketOf(const GroupSocket& s, bool input) {
    SocketDef d;
    d.id = s.id;
    d.displayName = s.name.empty() ? s.id : s.name;
    d.type = s.type;
    if (isValueType(s.type)) {
        Value v = zeroValue(s.type);
        Value parsed;
        if (!s.defaultValue.is_null() && paramFromJson(s.type, s.defaultValue, parsed)) v = parsed;
        d.defaultValue = v;
    } else if (input) {
        d.required = true;  // geometry and fields have no default: they must be connected
    }
    return d;
}

uint64_t fnv(uint64_t h, const std::string& s) {
    for (unsigned char c : s) h = (h ^ c) * 1099511628211ull;
    return h;
}

uint64_t hashOf(const GroupDef& d) {
    uint64_t h = 1469598103934665603ull;
    h = fnv(h, d.name);
    for (const auto* list : {&d.inputs, &d.outputs}) {
        h = fnv(h, "|");
        for (const GroupSocket& s : *list) h = fnv(fnv(fnv(h, s.id), toString(s.type)), s.defaultValue.dump());
    }
    if (d.graph) h = fnv(h, graphToJson(*d.graph).dump());
    return h;
}

void addGroupDefinitions(NodeRegistry& r, const LibraryPtr& lib);

// The registry the inside of group `index` is evaluated with: built-ins, every group of the library, and this group's
// GroupInput / GroupOutput (whose sockets are the interface).
NodeRegistry innerRegistry(const LibraryPtr& lib, size_t index) {
    NodeRegistry r = NodeRegistry::builtin();
    addGroupDefinitions(r, lib);
    const GroupDef& g = (*lib)[index];
    {
        NodeDefinition d;
        d.typeId = "GroupInput";
        d.displayName = "Group Input";
        d.category = "Groups";
        d.description = "The interface inputs of the group this graph belongs to.";
        for (const GroupSocket& s : g.inputs) d.outputs.push_back(socketOf(s, false));
        std::vector<GroupSocket> sockets = g.inputs;
        d.evaluate = [sockets](NodeEvalContext& c) {
            const auto* values = c.eval().groupInputs;
            for (const GroupSocket& s : sockets) {
                const auto it = values ? values->find(s.id) : decltype(values->end())();
                if (!values || it == values->end()) {
                    c.fail(DiagCode::MissingInput, "group input '" + s.id + "' has no value", s.id);
                    return;
                }
                c.setOutput(s.id, it->second);
            }
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "GroupOutput";
        d.displayName = "Group Output";
        d.category = "Groups";
        d.description = "The interface outputs of the group this graph belongs to.";
        d.isGroupOutput = true;
        for (const GroupSocket& s : g.outputs) {
            d.inputs.push_back(socketOf(s, true));
            d.outputs.push_back(socketOf(s, false));
        }
        std::vector<GroupSocket> sockets = g.outputs;
        d.evaluate = [sockets](NodeEvalContext& c) {
            for (const GroupSocket& s : sockets) {
                const auto it = c.inputs.find(s.id);
                if (it == c.inputs.end() || it->second.empty() || std::holds_alternative<std::monostate>(it->second[0])) {
                    c.fail(DiagCode::MissingInput, "group output '" + s.id + "' is not connected", s.id);
                    return;
                }
                c.setOutput(s.id, it->second[0]);
            }
        };
        r.add(std::move(d));
    }
    return r;
}

void addGroupDefinitions(NodeRegistry& r, const LibraryPtr& lib) {
    for (size_t i = 0; i < lib->size(); ++i) {
        const GroupDef& g = (*lib)[i];
        if (!g.graph) continue;
        NodeDefinition d;
        d.typeId = groupNodeType(g.name);
        d.displayName = g.name;
        d.category = "Groups";
        d.description = "Node group '" + g.name + "'.";
        for (const GroupSocket& s : g.inputs) d.inputs.push_back(socketOf(s, true));
        for (const GroupSocket& s : g.outputs) d.outputs.push_back(socketOf(s, false));
        d.contentHash = hashOf(g);
        d.evaluate = [lib, i](NodeEvalContext& c) {
            const GroupDef& def = (*lib)[i];
            if (c.eval().groupDepth >= kMaxGroupDepth) {
                c.fail(DiagCode::LimitExceeded, "group '" + def.name + "': nesting deeper than " + std::to_string(kMaxGroupDepth) +
                                                    " levels (does a group contain itself?)");
                return;
            }
            std::map<std::string, Value> inputs;
            for (const GroupSocket& s : def.inputs) {
                const auto it = c.inputs.find(s.id);
                if (it != c.inputs.end() && !it->second.empty()) inputs[s.id] = it->second[0];
            }
            EvalContext inner;
            inner.seed = c.seed();
            inner.limits = c.limits();
            inner.cancel = c.cancelFlag();
            inner.groupInputs = &inputs;
            inner.groupDepth = c.eval().groupDepth + 1;
            const NodeRegistry registry = innerRegistry(lib, i);
            const EvalResult r = evaluateGraph(*def.graph, registry, inner, nullptr);
            if (!r.success) {
                const Diagnostic* first = nullptr;
                for (const Diagnostic& diag : r.diagnostics) {
                    if (diag.severity != Severity::Error) continue;
                    if (!first || (first->code == DiagCode::Upstream && diag.code != DiagCode::Upstream)) first = &diag;
                }
                if (!first) {
                    c.fail(DiagCode::Internal, "group '" + def.name + "' failed");
                } else {
                    std::string message = "group '" + def.name + "': ";
                    if (first->node != 0) message += "node " + std::to_string(first->node) + ": ";
                    c.fail(first->code, message + first->message);
                }
                return;
            }
            for (const GroupSocket& s : def.outputs) {
                const auto it = r.values.find(s.id);
                if (it == r.values.end()) {
                    c.fail(DiagCode::Internal, "group '" + def.name + "' did not produce '" + s.id + "'");
                    return;
                }
                c.setOutput(s.id, it->second);
            }
        };
        r.add(std::move(d));
    }
}

NodeId maxNodeId(const Graph& g) {
    NodeId m = 0;
    for (const Node& n : g.nodes) m = std::max(m, n.id);
    return m;
}

std::string uniqueSocketId(const std::vector<GroupSocket>& list, std::string base) {
    for (char& ch : base)
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') ch = '_';
    std::string id = base;
    for (int n = 2;; ++n) {
        const bool used = std::any_of(list.begin(), list.end(), [&](const GroupSocket& s) { return s.id == id; });
        if (!used) return id;
        id = base + "_" + std::to_string(n);
    }
}

}  // namespace

bool isGroupNodeType(const std::string& type) { return type.rfind(kGroupNodePrefix, 0) == 0 && type.size() > 6; }

std::string groupNodeType(const std::string& groupName) { return kGroupNodePrefix + groupName; }

std::string groupNameOf(const std::string& nodeType) { return isGroupNodeType(nodeType) ? nodeType.substr(6) : std::string(); }

NodeRegistry registryFor(const Graph& graph) {
    NodeRegistry r = NodeRegistry::builtin();
    if (graph.groups.empty()) return r;
    addGroupDefinitions(r, std::make_shared<const GroupLibrary>(graph.groups));
    return r;
}

const GroupDef* findGroup(const Graph& graph, const std::string& name) {
    for (const GroupDef& g : graph.groups)
        if (g.name == name) return &g;
    return nullptr;
}

bool makeGroup(Graph& graph, const std::string& name, const std::vector<NodeId>& selection, std::string* error, NodeId* groupNode) {
    if (!validGroupName(name)) {
        setError(error, "group name must be non-empty and use letters, digits, underscore or hyphen");
        return false;
    }
    if (findGroup(graph, name)) {
        setError(error, "a group named '" + name + "' already exists");
        return false;
    }
    const std::set<NodeId> sel(selection.begin(), selection.end());
    if (sel.empty() || sel.size() != selection.size()) {
        setError(error, "select one or more distinct nodes");
        return false;
    }
    const NodeRegistry registry = registryFor(graph);
    for (NodeId id : sel) {
        const Node* n = graph.findNode(id);
        if (!n) {
            setError(error, "node " + std::to_string(id) + " does not exist");
            return false;
        }
        const NodeDefinition* def = registry.find(n->type);
        if (!def) {
            setError(error, "node " + std::to_string(id) + " has an unknown type '" + n->type + "'");
            return false;
        }
        if (def->isOutput) {
            setError(error, "the Output node cannot be part of a group");
            return false;
        }
    }
    for (const ExposedParam& e : graph.exposed) {
        if (sel.count(e.node)) {
            setError(error, "node " + std::to_string(e.node) + " has a public parameter ('" + e.name + "'); unexpose it first");
            return false;
        }
    }

    Graph inner;
    inner.seed = graph.seed;
    double minX = 1e300, maxX = -1e300, sumY = 0, sumX = 0;
    int placed = 0;
    for (const Node& n : graph.nodes) {
        if (!sel.count(n.id)) continue;
        inner.nodes.push_back(n);
        const auto lay = graph.layout.find(n.id);
        if (lay != graph.layout.end()) {
            inner.layout[n.id] = lay->second;
            minX = std::min(minX, lay->second.x);
            maxX = std::max(maxX, lay->second.x);
            sumX += lay->second.x;
            sumY += lay->second.y;
            ++placed;
        }
    }
    if (placed == 0) minX = maxX = 0;

    GroupDef def;
    def.name = name;
    NodeId innerNext = maxNodeId(inner) + 1;
    NodeId groupInput = 0, groupOutput = 0;
    std::map<std::pair<NodeId, std::string>, std::string> outputIds;  // inside (node, output socket) -> interface output
    std::vector<Link> outerLinks;                                      // node id 0 stands for the new group node
    for (const Link& l : graph.links) {
        const bool fromIn = sel.count(l.from.node) != 0, toIn = sel.count(l.to.node) != 0;
        if (fromIn && toIn) {
            inner.links.push_back(l);
        } else if (!fromIn && toIn) {
            const Node* target = graph.findNode(l.to.node);
            const NodeDefinition* tdef = target ? registry.find(target->type) : nullptr;
            const SocketDef* ts = tdef ? tdef->findInput(l.to.socket) : nullptr;
            if (!ts) {
                setError(error, "a link enters node " + std::to_string(l.to.node) + " at an unknown socket '" + l.to.socket + "'");
                return false;
            }
            GroupSocket gs;
            gs.id = uniqueSocketId(def.inputs, target->type + "_" + l.to.socket);
            gs.name = tdef->displayName + " " + ts->displayName;
            gs.type = ts->type;
            def.inputs.push_back(gs);
            if (!groupInput) {
                groupInput = innerNext++;
                Node gi;
                gi.id = groupInput;
                gi.type = "GroupInput";
                inner.nodes.push_back(gi);
            }
            inner.links.push_back(Link{Endpoint{groupInput, gs.id}, l.to});
            outerLinks.push_back(Link{l.from, Endpoint{0, gs.id}});
        } else if (fromIn && !toIn) {
            const auto key = std::make_pair(l.from.node, l.from.socket);
            auto it = outputIds.find(key);
            if (it == outputIds.end()) {
                const Node* source = graph.findNode(l.from.node);
                const NodeDefinition* sdef = source ? registry.find(source->type) : nullptr;
                const SocketDef* ss = sdef ? sdef->findOutput(l.from.socket) : nullptr;
                if (!ss) {
                    setError(error, "a link leaves node " + std::to_string(l.from.node) + " from an unknown socket '" + l.from.socket + "'");
                    return false;
                }
                GroupSocket gs;
                gs.id = uniqueSocketId(def.outputs, source->type + "_" + l.from.socket);
                gs.name = sdef->displayName + " " + ss->displayName;
                gs.type = ss->type;
                def.outputs.push_back(gs);
                if (!groupOutput) {
                    groupOutput = innerNext++;
                    Node go;
                    go.id = groupOutput;
                    go.type = "GroupOutput";
                    inner.nodes.push_back(go);
                }
                inner.links.push_back(Link{l.from, Endpoint{groupOutput, gs.id}});
                it = outputIds.emplace(key, gs.id).first;
            }
            outerLinks.push_back(Link{Endpoint{0, it->second}, l.to});
        } else {
            outerLinks.push_back(l);
        }
    }
    if (def.outputs.empty()) {
        setError(error, "the selection has no connection to the rest of the graph (nothing leaves it), so a group would produce nothing");
        return false;
    }
    const double midY = placed ? sumY / placed : 0.0;
    if (groupInput) inner.layout[groupInput] = NodeLayout{minX - 260.0, midY};
    if (groupOutput) inner.layout[groupOutput] = NodeLayout{maxX + 260.0, midY};
    def.graph = std::make_shared<const Graph>(std::move(inner));

    Graph result = graph;
    result.nodes.erase(std::remove_if(result.nodes.begin(), result.nodes.end(), [&](const Node& n) { return sel.count(n.id) != 0; }),
                       result.nodes.end());
    for (NodeId id : sel) result.layout.erase(id);
    result.groups.push_back(std::move(def));
    Node& groupNodeRef = addNode(result, groupNodeType(name), 1);
    const NodeId groupId = groupNodeRef.id;
    for (Link& l : outerLinks) {
        if (l.from.node == 0) l.from.node = groupId;
        if (l.to.node == 0) l.to.node = groupId;
    }
    result.links = std::move(outerLinks);
    if (placed) result.layout[groupId] = NodeLayout{sumX / placed, sumY / placed};

    for (const Diagnostic& d : validateGraph(result, registryFor(result))) {
        if (d.code == DiagCode::Cycle) {
            setError(error, "grouping these nodes would create a cycle (a node outside the selection sits between them)");
            return false;
        }
    }
    graph = std::move(result);
    if (groupNode) *groupNode = groupId;
    return true;
}

bool ungroup(Graph& graph, NodeId groupNode, std::string* error) {
    const Node* node = graph.findNode(groupNode);
    if (!node || !isGroupNodeType(node->type)) {
        setError(error, "node " + std::to_string(groupNode) + " is not a group node");
        return false;
    }
    const GroupDef* def = findGroup(graph, groupNameOf(node->type));
    if (!def || !def->graph) {
        setError(error, "the group '" + groupNameOf(node->type) + "' is not defined in this graph");
        return false;
    }
    const Graph& inner = *def->graph;
    const Node groupCopy = *node;
    const NodeLayout groupPos = graph.layout.count(groupNode) ? graph.layout[groupNode] : NodeLayout{};

    // New ids for the inner nodes (the GroupInput / GroupOutput markers disappear).
    NodeId next = std::max(maxNodeId(graph), maxNodeId(inner)) + 1;
    std::map<NodeId, NodeId> remap;
    NodeId innerInput = 0, innerOutput = 0;
    for (const Node& n : inner.nodes) {
        if (n.type == "GroupInput") innerInput = n.id;
        else if (n.type == "GroupOutput") innerOutput = n.id;
        else remap[n.id] = next++;
    }
    auto mapped = [&](const Endpoint& e) { return Endpoint{remap.at(e.node), e.socket}; };

    // What feeds each interface input from outside (in link order), and what each interface output feeds.
    std::map<std::string, std::vector<Endpoint>> feeders;
    std::vector<Link> links;
    std::vector<Link> consumers;  // groupNode.<out> -> target
    for (const Link& l : graph.links) {
        if (l.to.node == groupNode) feeders[l.to.socket].push_back(l.from);
        else if (l.from.node == groupNode) consumers.push_back(l);
        else links.push_back(l);
    }

    std::vector<Node> newNodes;
    for (const Node& n : inner.nodes) {
        if (n.id == innerInput || n.id == innerOutput) continue;
        Node copy = n;
        copy.id = remap.at(n.id);
        newNodes.push_back(std::move(copy));
    }
    auto target = [&](NodeId innerId) -> Node& {
        for (Node& n : newNodes)
            if (n.id == remap.at(innerId)) return n;
        return newNodes.front();
    };

    std::map<std::string, Endpoint> outputSource;
    for (const Link& l : inner.links) {
        if (l.from.node == innerInput) {
            const auto f = feeders.find(l.from.socket);
            if (f != feeders.end() && !f->second.empty()) {
                for (const Endpoint& src : f->second) links.push_back(Link{src, mapped(l.to)});
            } else {
                // Unconnected interface input: its value (the group node's parameter, else the interface default)
                // becomes the parameter of the inner socket it fed.
                nlohmann::json value;
                const auto p = groupCopy.params.find(l.from.socket);
                if (p != groupCopy.params.end()) value = p->second;
                else
                    for (const GroupSocket& s : def->inputs)
                        if (s.id == l.from.socket) value = s.defaultValue;
                if (!value.is_null()) target(l.to.node).params[l.to.socket] = value;
            }
        } else if (l.to.node == innerOutput) {
            outputSource[l.to.socket] = mapped(l.from);
        } else {
            links.push_back(Link{mapped(l.from), mapped(l.to)});
        }
    }
    for (const Link& l : consumers) {
        const auto src = outputSource.find(l.from.socket);
        if (src != outputSource.end()) links.push_back(Link{src->second, l.to});
    }

    // Inner layout around the group node's position.
    double cx = 0, cy = 0;
    int count = 0;
    for (const Node& n : inner.nodes) {
        const auto lay = inner.layout.find(n.id);
        if (n.id == innerInput || n.id == innerOutput || lay == inner.layout.end()) continue;
        cx += lay->second.x;
        cy += lay->second.y;
        ++count;
    }
    if (count) { cx /= count; cy /= count; }

    removeNode(graph, groupNode);
    for (Node& n : newNodes) {
        const NodeId innerId = [&] {
            for (const auto& [from, to] : remap) if (to == n.id) return from;
            return NodeId(0);
        }();
        const auto lay = inner.layout.find(innerId);
        if (lay != inner.layout.end()) graph.layout[n.id] = NodeLayout{groupPos.x + lay->second.x - cx, groupPos.y + lay->second.y - cy};
        graph.nodes.push_back(std::move(n));
    }
    graph.links = std::move(links);
    return true;
}

bool removeGroup(Graph& graph, const std::string& name, std::string* error) {
    const std::string type = groupNodeType(name);
    for (auto it = graph.groups.begin(); it != graph.groups.end(); ++it) {
        if (it->name != name) continue;
        for (const Node& n : graph.nodes) {
            if (n.type == type) {
                setError(error, "group '" + name + "' is used by node " + std::to_string(n.id));
                return false;
            }
        }
        for (const GroupDef& other : graph.groups) {
            if (other.name == name || !other.graph) continue;
            for (const Node& n : other.graph->nodes) {
                if (n.type == type) {
                    setError(error, "group '" + name + "' is used inside group '" + other.name + "'");
                    return false;
                }
            }
        }
        graph.groups.erase(it);
        return true;
    }
    setError(error, "no group named '" + name + "'");
    return false;
}

}  // namespace Phantom::GeometryNode
