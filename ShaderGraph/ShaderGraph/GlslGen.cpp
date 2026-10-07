#include <algorithm>
#include <map>
#include <set>

#include "ShaderGraph.h"

namespace Phantom::ShaderGraph {

namespace {

const char* kSwizzle[] = {"", ".x", ".xy", ".xyz", ""};

std::string varName(NodeId id, const std::string& socket) {
    return "n" + std::to_string(id) + "_" + socket;
}

struct Gen {
    const Graph& g;
    CompileResult& r;
    std::map<std::pair<NodeId, std::string>, const Link*> incoming;
    std::map<std::pair<NodeId, std::string>, int> slotOf;
    std::map<std::pair<std::string, bool>, int> textureOf;
    std::set<NodeId> visited;
    std::vector<NodeId> order;
    std::map<NodeId, NodeSignature> sig;

    void visit(NodeId id) {
        if (!visited.insert(id).second) return;
        const NodeSignature& s = sig[id];
        for (const SocketDef& in : s.inputs) {
            auto it = incoming.find({id, in.name});
            if (it != incoming.end()) visit(it->second->fromNode);
        }
        order.push_back(id);
    }

    int slot(const Node& n, const SocketDef& d) {
        auto key = std::make_pair(n.id, d.name);
        auto it = slotOf.find(key);
        if (it != slotOf.end()) return it->second;
        const int s = static_cast<int>(r.params.size());
        r.params.push_back({s, n.id, d.name, d.type});
        slotOf[key] = s;
        return s;
    }

    std::string slotExpr(const Node& n, const SocketDef& d) {
        return "sg_p[" + std::to_string(slot(n, d)) + "]" + kSwizzle[socketComponents(d.type)];
    }

    std::string input(const Node& n, const SocketDef& d) {
        auto it = incoming.find({n.id, d.name});
        if (it != incoming.end()) return varName(it->second->fromNode, it->second->fromSocket);
        if (d.hasDefault) return slotExpr(n, d);
        return d.type == SocketType::Normal ? "tbn[2]" : "uv";
    }

    int texture(const Node& n) {
        auto p = n.params.find("Path");
        const std::string path = (p != n.params.end() && p->second.is_string()) ? p->second.get<std::string>() : "";
        auto c = n.params.find("ColorSpace");
        const bool srgb = !(c != n.params.end() && c->second == "Linear");
        auto key = std::make_pair(path, srgb);
        auto it = textureOf.find(key);
        if (it != textureOf.end()) return it->second;
        const int idx = static_cast<int>(r.textures.size());
        r.textures.push_back({idx, n.id, path, srgb});
        textureOf[key] = idx;
        return idx;
    }

    // Emits the GLSL statements for one node.
    std::vector<std::string> emit(const Node& n) {
        const NodeSignature& s = sig[n.id];
        std::vector<std::string> L;
        auto v = [&](const std::string& sock) { return varName(n.id, sock); };
        auto in = [&](size_t i) { return input(n, s.inputs[i]); };
        const std::string& t = n.type;
        if (t == "Float" || t == "Color" || t == "Vector") {
            const SocketDef& o = s.outputs[0];
            L.push_back(std::string(glslTypeName(o.type)) + " " + v(o.name) + " = " + slotExpr(n, s.valueParams[0]) + ";");
        } else if (t == "UV") {
            L.push_back("vec2 " + v("UV") + " = uv;");
        } else if (t == "ImageTexture") {
            const std::string tv = "n" + std::to_string(n.id) + "_t";
            L.push_back("vec4 " + tv + " = texture(sg_tex" + std::to_string(texture(n)) + ", " + in(0) + ");");
            L.push_back("vec3 " + v("Color") + " = " + tv + ".rgb;");
            L.push_back("float " + v("Alpha") + " = " + tv + ".a;");
        } else if (t == "Add" || t == "Multiply") {
            L.push_back(std::string(glslTypeName(s.outputs[0].type)) + " " + v("Result") + " = " + in(0) +
                        (t == "Add" ? " + " : " * ") + in(1) + ";");
        } else if (t == "Mix") {
            L.push_back(std::string(glslTypeName(s.outputs[0].type)) + " " + v("Result") + " = mix(" + in(0) + ", " +
                        in(1) + ", " + in(2) + ");");
        } else if (t == "Clamp") {
            const std::string lo = in(1), hi = in(2);
            L.push_back("float " + v("Result") + " = clamp(" + in(0) + ", min(" + lo + ", " + hi + "), max(" + lo +
                        ", " + hi + "));");
        } else if (t == "Split") {
            const std::string src = in(0);
            L.push_back("float " + v("X") + " = (" + src + ").x;");
            L.push_back("float " + v("Y") + " = (" + src + ").y;");
            L.push_back("float " + v("Z") + " = (" + src + ").z;");
        } else if (t == "Combine") {
            L.push_back("vec3 " + v("Vector") + " = vec3(" + in(0) + ", " + in(1) + ", " + in(2) + ");");
        } else if (t == "ToColor") {
            L.push_back("vec3 " + v("Color") + " = " + in(0) + ";");
        } else if (t == "ToVector") {
            L.push_back("vec3 " + v("Vector") + " = " + in(0) + ";");
        } else if (t == "NormalMap") {
            const std::string tv = "n" + std::to_string(n.id) + "_t";
            const std::string wv = "n" + std::to_string(n.id) + "_w";
            L.push_back("vec3 " + tv + " = " + in(0) + " * 2.0 - 1.0;");
            L.push_back(tv + ".xy *= " + in(1) + ";");
            L.push_back("vec3 " + wv + " = tbn * " + tv + ";");
            L.push_back("float " + wv + "l = length(" + wv + ");");
            L.push_back("vec3 " + v("Normal") + " = " + wv + "l > 1e-6 ? " + wv + " / " + wv + "l : tbn[2];");
        } else if (t == "SurfaceOutput") {
            L.push_back("s.baseColor = " + in(0) + ";");
            L.push_back("s.metallic = clamp(" + in(1) + ", 0.0, 1.0);");
            L.push_back("s.roughness = clamp(" + in(2) + ", 0.0, 1.0);");
            L.push_back("s.normal = " + in(3) + ";");
        }
        return L;
    }
};

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    return h;
}

}  // namespace

CompileResult compileGraph(const Graph& g) {
    CompileResult r;
    r.diagnostics = validateGraph(g);
    if (hasErrors(r.diagnostics)) return r;

    Gen gen{g, r, {}, {}, {}, {}, {}, {}};
    NodeId outputId = 0;
    for (const Node& n : g.nodes) {
        std::string err;
        resolveSignature(n, gen.sig[n.id], err);  // validated above (unknown unreachable nodes fail harmlessly)
        if (n.type == "SurfaceOutput") outputId = n.id;
    }
    for (const Link& l : g.links) gen.incoming[{l.toNode, l.toSocket}] = &l;
    gen.visit(outputId);

    std::vector<std::string> body;
    std::vector<LineRange> rel;  // body-relative, 0-based
    for (NodeId id : gen.order) {
        const std::vector<std::string> lines = gen.emit(*g.findNode(id));
        rel.push_back({id, static_cast<int>(body.size()), static_cast<int>(body.size() + lines.size()) - 1});
        for (const std::string& l : lines) body.push_back("    " + l);
    }

    std::vector<std::string> head;
    head.push_back("// generated by Phantom::ShaderGraph");
    head.push_back("struct SGSurface { vec3 baseColor; float metallic; float roughness; vec3 normal; };");
    head.push_back("layout(std140, set = 1, binding = 0) uniform SGParams { vec4 sg_p[" +
                   std::to_string(r.params.empty() ? 1 : r.params.size()) + "]; };");
    for (const TextureBinding& t : r.textures)
        head.push_back("layout(set = 1, binding = " + std::to_string(t.index + 1) + ") uniform sampler2D sg_tex" +
                       std::to_string(t.index) + ";");
    head.push_back("void sg_evaluate(vec2 uv, mat3 tbn, out SGSurface s) {");

    const int firstBody = static_cast<int>(head.size()) + 1;  // 1-based line of body[0]
    for (const LineRange& lr : rel)
        r.lineMap.push_back({lr.node, firstBody + lr.firstLine, firstBody + lr.lastLine});
    for (const std::string& l : head) r.glsl += l + "\n";
    for (const std::string& l : body) r.glsl += l + "\n";
    r.glsl += "}\n";

    if (r.glsl.size() > kMaxGlslBytes) {
        r.diagnostics.push_back({Severity::Error, 0, "limit.glsl", "generated code exceeds size limit"});
        return r;
    }
    r.cacheKey = fnv1a(r.glsl);
    r.ok = true;
    return r;
}

std::vector<float> packParameters(const Graph& g, const CompileResult& r) {
    std::vector<float> out(std::max<size_t>(r.params.size(), 1) * 4, 0.0f);
    for (const ParamSlot& p : r.params) {
        const Node* n = g.findNode(p.node);
        if (!n) continue;
        NodeSignature s;
        std::string err;
        if (!resolveSignature(*n, s, err)) continue;
        Vec4f v{};
        const int comps = socketComponents(p.type);
        bool found = false;
        for (const auto* list : {&s.valueParams, &s.inputs})
            for (const SocketDef& d : *list)
                if (!found && d.name == p.name) { v = d.def; found = true; }
        auto it = n->params.find(p.name);
        if (it != n->params.end()) vec4FromJson(it->second, comps, v);
        for (int i = 0; i < 4; ++i) out[static_cast<size_t>(p.slot) * 4 + i] = v[i];
    }
    return out;
}

NodeId nodeForGlslLine(const CompileResult& r, int line) {
    for (const LineRange& lr : r.lineMap)
        if (line >= lr.firstLine && line <= lr.lastLine) return lr.node;
    return 0;
}

}  // namespace Phantom::ShaderGraph
