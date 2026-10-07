#include <map>
#include <set>

#include "ShaderGraph.h"

namespace Phantom::ShaderGraph {

namespace {

void add(std::vector<Diagnostic>& d, Severity s, NodeId node, const char* code, std::string msg) {
    d.push_back({s, node, code, std::move(msg)});
}

const SocketDef* findSocket(const std::vector<SocketDef>& v, const std::string& name) {
    for (const SocketDef& s : v) if (s.name == name) return &s;
    return nullptr;
}

}  // namespace

bool hasErrors(const std::vector<Diagnostic>& d) {
    for (const Diagnostic& x : d) if (x.severity == Severity::Error) return true;
    return false;
}

std::vector<Diagnostic> validateGraph(const Graph& g) {
    std::vector<Diagnostic> out;

    if (static_cast<int>(g.nodes.size()) > kMaxNodes)
        add(out, Severity::Error, 0, "limit.nodes", "node count exceeds " + std::to_string(kMaxNodes));

    // ids, signatures
    std::map<NodeId, NodeSignature> sig;
    std::set<NodeId> unknown, seen;
    NodeId outputId = 0;
    int outputs = 0;
    for (const Node& n : g.nodes) {
        if (n.id <= 0) {
            add(out, Severity::Error, n.id, "node.id", "node id must be positive");
            continue;
        }
        if (!seen.insert(n.id).second) {
            add(out, Severity::Error, n.id, "node.duplicate", "duplicate node id " + std::to_string(n.id));
            continue;
        }
        if (n.type == "SurfaceOutput") { ++outputs; outputId = n.id; }
        NodeSignature s;
        std::string err;
        if (!resolveSignature(n, s, err)) {
            if (!isKnownNodeType(n.type)) unknown.insert(n.id);
            else add(out, Severity::Error, n.id, "node.params", err);
            continue;
        }
        // value overrides must be well-formed numbers
        auto checkValue = [&](const SocketDef& d) {
            auto it = n.params.find(d.name);
            if (it == n.params.end()) return;
            Vec4f v;
            if (!vec4FromJson(it->second, socketComponents(d.type), v))
                add(out, Severity::Error, n.id, "param.value",
                    "parameter '" + d.name + "' must be " + std::to_string(socketComponents(d.type)) +
                        " finite number(s)");
        };
        for (const SocketDef& d : s.valueParams) checkValue(d);
        for (const SocketDef& d : s.inputs) if (d.hasDefault) checkValue(d);
        if (n.type == "ImageTexture") {
            auto p = n.params.find("Path");
            if (p != n.params.end() && !p->second.is_string())
                add(out, Severity::Error, n.id, "param.value", "Path must be a string");
            auto c = n.params.find("ColorSpace");
            if (c != n.params.end() &&
                (!c->second.is_string() || (c->second != "sRGB" && c->second != "Linear")))
                add(out, Severity::Error, n.id, "param.value", "ColorSpace must be \"sRGB\" or \"Linear\"");
        }
        sig[n.id] = std::move(s);
    }
    if (outputs == 0) add(out, Severity::Error, 0, "output.missing", "graph has no SurfaceOutput node");
    if (outputs > 1) add(out, Severity::Error, 0, "output.multiple", "graph has more than one SurfaceOutput node");

    // links
    std::map<std::pair<NodeId, std::string>, int> inputCount;
    std::map<NodeId, std::vector<NodeId>> deps;  // node -> nodes it reads from (valid links only)
    for (const Link& l : g.links) {
        const bool fromOk = seen.count(l.fromNode) != 0, toOk = seen.count(l.toNode) != 0;
        if (!fromOk || !toOk) {
            add(out, Severity::Error, fromOk ? l.toNode : l.fromNode, "link.endpoint",
                "link refers to missing node " + std::to_string(fromOk ? l.toNode : l.fromNode));
            continue;
        }
        deps[l.toNode].push_back(l.fromNode);
        if (++inputCount[{l.toNode, l.toSocket}] == 2)
            add(out, Severity::Error, l.toNode, "link.multiple",
                "input '" + l.toSocket + "' has more than one incoming link");
        auto fs = sig.find(l.fromNode);
        auto ts = sig.find(l.toNode);
        if (fs == sig.end() || ts == sig.end()) continue;  // unknown type: reported by reachability below
        const SocketDef* o = findSocket(fs->second.outputs, l.fromSocket);
        const SocketDef* i = findSocket(ts->second.inputs, l.toSocket);
        if (!o) { add(out, Severity::Error, l.fromNode, "link.socket", "no output socket '" + l.fromSocket + "'"); continue; }
        if (!i) { add(out, Severity::Error, l.toNode, "link.socket", "no input socket '" + l.toSocket + "'"); continue; }
        if (o->type != i->type)
            add(out, Severity::Error, l.toNode, "link.type",
                std::string("cannot connect ") + socketTypeName(o->type) + " to " + socketTypeName(i->type) +
                    " input '" + l.toSocket + "'");
    }

    // cycles (iterative colouring over dependency edges)
    {
        std::map<NodeId, int> color;  // 0 white, 1 grey, 2 black
        bool cyc = false;
        for (const auto& [start, unusedDeps] : deps) {
            (void)unusedDeps;
            if (color[start] != 0 || cyc) continue;
            std::vector<std::pair<NodeId, size_t>> stack{{start, 0}};
            color[start] = 1;
            while (!stack.empty() && !cyc) {
                auto& [id, idx] = stack.back();
                const std::vector<NodeId>& d = deps[id];
                if (idx >= d.size()) { color[id] = 2; stack.pop_back(); continue; }
                const NodeId next = d[idx++];
                if (color[next] == 1) {
                    add(out, Severity::Error, next, "graph.cycle", "graph contains a cycle");
                    cyc = true;
                } else if (color[next] == 0) {
                    color[next] = 1;
                    stack.push_back({next, 0});
                }
            }
        }
    }

    // reachability from the output: unknown reachable nodes are errors, textures are counted
    std::set<NodeId> reach;
    if (outputs == 1) {
        std::vector<NodeId> stack{outputId};
        while (!stack.empty()) {
            const NodeId id = stack.back();
            stack.pop_back();
            if (!reach.insert(id).second) continue;
            auto it = deps.find(id);
            if (it != deps.end()) for (NodeId d : it->second) stack.push_back(d);
        }
    }
    for (NodeId id : unknown) {
        const Node* n = g.findNode(id);
        const bool r = reach.count(id) != 0;
        add(out, r ? Severity::Error : Severity::Warning, id, "node.unknown",
            "unknown node type '" + (n ? n->type : std::string()) + "'" + (r ? "" : " (unused, preserved)"));
    }
    int textures = 0;
    for (NodeId id : reach) {
        const Node* n = g.findNode(id);
        if (n && n->type == "ImageTexture") ++textures;
    }
    if (textures > kMaxTextures)
        add(out, Severity::Error, 0, "limit.textures", "texture count exceeds " + std::to_string(kMaxTextures));
    return out;
}

}  // namespace Phantom::ShaderGraph
