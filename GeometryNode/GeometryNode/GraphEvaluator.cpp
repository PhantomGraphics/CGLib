#include "GraphEvaluator.h"

#include <cstring>
#include <set>
#include <utility>

#include "GeometryOps.h"

namespace Phantom::GeometryNode {

namespace {

Diagnostic makeDiag(Severity sev, DiagCode code, NodeId node, std::string socket, std::string message) {
    Diagnostic d;
    d.severity = sev;
    d.code = code;
    d.node = node;
    d.socket = std::move(socket);
    d.message = std::move(message);
    return d;
}

struct Analysis {
    std::vector<Diagnostic> diags;
    std::map<NodeId, const Node*> byId;
    std::map<NodeId, const NodeDefinition*> defs;  // null for unknown types
    std::vector<bool> linkValid;
    // Valid links into an input endpoint, in graph link order.
    std::map<std::pair<NodeId, std::string>, std::vector<size_t>> incoming;
    NodeId output = 0;
    std::vector<const Node*> order;  // reachable nodes, dependencies first
};

Analysis analyze(const Graph& graph, const NodeRegistry& registry) {
    Analysis a;

    // 1. Node ids and definitions.
    for (const Node& n : graph.nodes) {
        if (n.id == 0) {
            a.diags.push_back(makeDiag(Severity::Error, DiagCode::InvalidNodeId, 0, {}, "node id 0 is reserved"));
            continue;
        }
        if (!a.byId.emplace(n.id, &n).second) {
            a.diags.push_back(makeDiag(Severity::Error, DiagCode::DuplicateNodeId, n.id, {},
                                       "node id " + std::to_string(n.id) + " is used more than once"));
            continue;
        }
        a.defs[n.id] = registry.find(n.type);
    }

    // 2. Links.
    a.linkValid.assign(graph.links.size(), false);
    for (size_t i = 0; i < graph.links.size(); ++i) {
        const Link& l = graph.links[i];
        const auto fromIt = a.byId.find(l.from.node);
        const auto toIt = a.byId.find(l.to.node);
        if (fromIt == a.byId.end()) {
            a.diags.push_back(makeDiag(Severity::Error, DiagCode::UnknownSocket, l.to.node, l.to.socket,
                                       "link source node " + std::to_string(l.from.node) + " does not exist"));
            continue;
        }
        if (toIt == a.byId.end()) {
            a.diags.push_back(makeDiag(Severity::Error, DiagCode::UnknownSocket, l.from.node, l.from.socket,
                                       "link target node " + std::to_string(l.to.node) + " does not exist"));
            continue;
        }
        const NodeDefinition* fromDef = a.defs[l.from.node];
        const NodeDefinition* toDef = a.defs[l.to.node];
        bool ok = true;
        const SocketDef* fromSock = nullptr;
        const SocketDef* toSock = nullptr;
        if (fromDef) {
            fromSock = fromDef->findOutput(l.from.socket);
            if (!fromSock) {
                a.diags.push_back(makeDiag(Severity::Error, DiagCode::UnknownSocket, l.from.node, l.from.socket,
                                           "node '" + fromDef->typeId + "' has no output socket '" + l.from.socket + "'"));
                ok = false;
            }
        }
        if (toDef) {
            toSock = toDef->findInput(l.to.socket);
            if (!toSock) {
                a.diags.push_back(makeDiag(Severity::Error, DiagCode::UnknownSocket, l.to.node, l.to.socket,
                                           "node '" + toDef->typeId + "' has no input socket '" + l.to.socket + "'"));
                ok = false;
            }
        }
        if (ok && fromSock && toSock && fromSock->type != toSock->type) {
            a.diags.push_back(makeDiag(Severity::Error, DiagCode::TypeMismatch, l.to.node, l.to.socket,
                                       std::string("cannot connect ") + toString(fromSock->type) + " output to " +
                                           toString(toSock->type) + " input (no implicit conversion)"));
            ok = false;
        }
        if (!ok) continue;
        a.linkValid[i] = true;
        a.incoming[{l.to.node, l.to.socket}].push_back(i);
    }
    for (const auto& [key, links] : a.incoming) {
        if (links.size() < 2) continue;
        const NodeDefinition* def = a.defs[key.first];
        const SocketDef* s = def ? def->findInput(key.second) : nullptr;
        if (s && !s->multi) {
            a.diags.push_back(makeDiag(Severity::Error, DiagCode::MultipleLinks, key.first, key.second,
                                       "input '" + key.second + "' accepts only one link"));
        }
    }

    // 3. Exactly one Output node.
    size_t outputs = 0;
    for (const Node& n : graph.nodes) {
        auto it = a.defs.find(n.id);
        if (it != a.defs.end() && it->second && it->second->isOutput && a.byId[n.id] == &n) {
            ++outputs;
            a.output = n.id;
        }
    }
    if (outputs == 0) {
        a.diags.push_back(makeDiag(Severity::Error, DiagCode::NoOutputNode, 0, {}, "graph has no Output node"));
        return a;
    }
    if (outputs > 1) {
        a.diags.push_back(makeDiag(Severity::Error, DiagCode::MultipleOutputNodes, 0, {}, "graph has more than one Output node"));
        return a;
    }

    // 4. Dependencies of reachable nodes (iterative DFS: no recursion depth limit).
    std::map<NodeId, std::vector<NodeId>> deps;
    for (size_t i = 0; i < graph.links.size(); ++i) {
        if (a.linkValid[i]) deps[graph.links[i].to.node].push_back(graph.links[i].from.node);
    }
    std::map<NodeId, int> state;  // 1 = on stack, 2 = done
    struct Frame {
        NodeId id;
        size_t next;
    };
    // Walk Output first to record evaluation order, then validate disconnected components.
    std::vector<NodeId> roots{a.output};
    for (const auto& [id, node] : a.byId) if (id != a.output) roots.push_back(id);
    for (NodeId root : roots) {
        if (state[root] != 0) continue;
        const bool reachable = root == a.output;
        std::vector<Frame> stack{{root, 0}};
        state[root] = 1;
        while (!stack.empty()) {
            Frame& f = stack.back();
            const std::vector<NodeId>& d = deps[f.id];
            if (f.next < d.size()) {
                const NodeId dep = d[f.next++];
                const int s = state[dep];
                if (s == 1) {
                    a.diags.push_back(makeDiag(Severity::Error, DiagCode::Cycle, dep, {}, "graph contains a cycle"));
                } else if (s == 0) {
                    state[dep] = 1;
                    stack.push_back({dep, 0});
                }
            } else {
                state[f.id] = 2;
                if (reachable) a.order.push_back(a.byId[f.id]);
                stack.pop_back();
            }
        }
    }

    // 5. Per-node checks, reachable nodes only.
    for (const Node* n : a.order) {
        const NodeDefinition* def = a.defs[n->id];
        if (!def) {
            a.diags.push_back(makeDiag(Severity::Error, DiagCode::UnknownNodeType, n->id, {},
                                       "unknown node type '" + n->type + "'"));
            continue;
        }
        if (n->version > def->version) {
            a.diags.push_back(makeDiag(Severity::Error, DiagCode::UnsupportedNodeVersion, n->id, {},
                                       "node '" + n->type + "' version " + std::to_string(n->version) +
                                           " is newer than supported version " + std::to_string(def->version)));
        }
        for (const auto& [name, json] : n->params) {
            const SocketDef* s = def->findInput(name);
            if (!s) {
                a.diags.push_back(makeDiag(Severity::Warning, DiagCode::UnknownParameter, n->id, name,
                                           "unknown parameter '" + name + "' is kept but ignored"));
                continue;
            }
            Value v;
            if (!paramFromJson(s->type, json, v)) {
                a.diags.push_back(makeDiag(Severity::Error, DiagCode::InvalidParameter, n->id, name,
                                           std::string("parameter '") + name + "' is not a valid " + toString(s->type)));
            }
        }
        for (const SocketDef& s : def->inputs) {
            if (s.required && !a.incoming.count({n->id, s.id})) {
                a.diags.push_back(makeDiag(Severity::Error, DiagCode::MissingInput, n->id, s.id,
                                           "required input '" + s.id + "' is not connected"));
            }
        }
    }
    return a;
}

// --- cache key ---------------------------------------------------------------

void appendBytes(std::string& key, const void* p, size_t n) { key.append(static_cast<const char*>(p), n); }

template <class T>
void appendPod(std::string& key, const T& v) {
    appendBytes(key, &v, sizeof(T));
}

void appendString(std::string& key, const std::string& s) {
    appendPod(key, static_cast<uint64_t>(s.size()));
    key += s;
}

void appendValue(std::string& key, const Value& v) {
    appendPod(key, static_cast<uint8_t>(v.index()));
    if (const float* f = std::get_if<float>(&v)) appendPod(key, *f);
    else if (const int32_t* i = std::get_if<int32_t>(&v)) appendPod(key, *i);
    else if (const bool* b = std::get_if<bool>(&v)) appendPod(key, *b);
    else if (const Vec3* p = std::get_if<Vec3>(&v)) appendPod(key, *p);
}

// Checks the declared outputs of a finished node: present, right type, valid
// and within limits. Returns false after pushing a diagnostic.
bool checkOutputs(const NodeDefinition& def, NodeEvalContext& ctx, const Limits& limits,
                  std::vector<Diagnostic>& diags) {
    auto check = [&](const std::string& id, SocketType type) {
        auto it = ctx.outputs().find(id);
        SocketType got;
        if (it == ctx.outputs().end() || !valueType(it->second, got) || got != type) {
            diags.push_back(makeDiag(Severity::Error, DiagCode::Internal, ctx.node(), id,
                                     "node '" + def.typeId + "' did not produce output '" + id + "'"));
            return false;
        }
        if (isFieldType(type)) return true;  // non-null by valueType()
        if (type == SocketType::Geometry) {
            const GeometryPtr& g = std::get<GeometryPtr>(it->second);
            if (!g) {
                diags.push_back(makeDiag(Severity::Error, DiagCode::InvalidGeometry, ctx.node(), id, "null geometry output"));
                return false;
            }
            if (!fitsLimits(g->positions.size(), g->indices.size(), limits)) {
                diags.push_back(makeDiag(Severity::Error, DiagCode::LimitExceeded, ctx.node(), id,
                                         "geometry output exceeds the resource limits"));
                return false;
            }
            std::string msg;
            if (validateMesh(*g, &msg) != OpStatus::Ok) {
                diags.push_back(makeDiag(Severity::Error, DiagCode::InvalidGeometry, ctx.node(), id, msg));
                return false;
            }
        }
        return true;
    };
    for (const SocketDef& s : def.outputs) {
        if (!check(s.id, s.type)) return false;
    }
    if (def.isOutput) return check("Geometry", SocketType::Geometry);
    return true;
}

}  // namespace

std::vector<Diagnostic> validateGraph(const Graph& graph, const NodeRegistry& registry) {
    return analyze(graph, registry).diags;
}

EvalResult evaluateGraph(const Graph& graph, const NodeRegistry& registry, const EvalContext& context,
                         EvalCache* cache) {
    EvalResult result;
    Analysis a = analyze(graph, registry);
    result.diagnostics = std::move(a.diags);
    result.outputNode = a.output;
    if (hasErrors(result.diagnostics)) return result;

    EvalCache localCache;
    EvalCache& store = cache ? *cache : localCache;

    // Drop entries of nodes that no longer exist (keeps a re-connected branch warm).
    for (auto it = store.entries.begin(); it != store.entries.end();) {
        if (!a.byId.count(it->first)) it = store.entries.erase(it);
        else ++it;
    }

    std::set<NodeId> failed;
    for (const Node* n : a.order) {
        if (context.cancelled()) {
            result.diagnostics.push_back(makeDiag(Severity::Error, DiagCode::Cancelled, 0, {}, "evaluation cancelled"));
            return result;
        }
        const NodeDefinition& def = *a.defs[n->id];

        // Resolve inputs and build the exact cache key.
        NodeEvalContext ctx(context, n->id);
        std::string key;
        appendString(key, def.typeId);
        appendPod(key, n->version);
        appendPod(key, context.seed);
        appendPod(key, context.limits.maxVertices);
        appendPod(key, context.limits.maxIndices);
        appendPod(key, context.limits.maxInstances);
        appendPod(key, context.limits.maxMemoryBytes);

        bool upstreamFailed = false;
        for (const SocketDef& s : def.inputs) {
            appendString(key, s.id);
            std::vector<Value>& values = ctx.inputs[s.id];
            auto linksIt = a.incoming.find({n->id, s.id});
            if (linksIt != a.incoming.end()) {
                for (size_t li : linksIt->second) {
                    const Link& l = graph.links[li];
                    if (failed.count(l.from.node)) {
                        upstreamFailed = true;
                        continue;
                    }
                    const EvalCache::Entry& up = store.entries.at(l.from.node);
                    appendPod(key, static_cast<uint8_t>('L'));
                    appendPod(key, l.from.node);
                    appendString(key, l.from.socket);
                    appendPod(key, up.revision);
                    values.push_back(up.outputs.at(l.from.socket));
                }
            } else if (s.type != SocketType::Geometry && !isFieldType(s.type)) {  // geometry/fields have no default: unlinked = absent
                Value v = s.defaultValue;
                auto pit = n->params.find(s.id);
                if (pit != n->params.end()) paramFromJson(s.type, pit->second, v);  // validated in analyze()
                appendPod(key, static_cast<uint8_t>('V'));
                appendValue(key, v);
                values.push_back(std::move(v));
            }
        }
        if (upstreamFailed) {
            failed.insert(n->id);
            store.entries.erase(n->id);
            result.diagnostics.push_back(makeDiag(Severity::Error, DiagCode::Upstream, n->id, {},
                                                  "not evaluated because an upstream node failed"));
            continue;
        }

        auto cached = store.entries.find(n->id);
        if (cached != store.entries.end() && cached->second.key == key) {
            ++result.stats.nodesCached;
            continue;
        }

        def.evaluate(ctx);
        ++result.stats.nodesEvaluated;
        std::vector<Diagnostic> nodeDiags = ctx.diagnostics();
        bool ok = !ctx.failed();
        if (ok) ok = checkOutputs(def, ctx, context.limits, nodeDiags);
        result.diagnostics.insert(result.diagnostics.end(), nodeDiags.begin(), nodeDiags.end());
        if (!ok) {
            failed.insert(n->id);
            store.entries.erase(n->id);
            continue;
        }
        EvalCache::Entry& e = store.entries[n->id];
        e.key = std::move(key);
        e.revision = store.nextRevision++;
        e.outputs = ctx.outputs();
    }

    if (failed.count(a.output)) return result;
    const EvalCache::Entry& out = store.entries.at(a.output);
    result.geometry = std::get<GeometryPtr>(out.outputs.at("Geometry"));
    result.success = true;
    return result;
}

}  // namespace Phantom::GeometryNode
