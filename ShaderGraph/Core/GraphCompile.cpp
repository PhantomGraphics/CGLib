#include "GraphCompile.h"

#include <algorithm>
#include <filesystem>
#include <mutex>

namespace Phantom::ShaderGraph {

namespace {

std::mutex& compilerMutex() { static std::mutex m; return m; }

RuntimeCompiler* sharedCompiler() {
    static RuntimeCompiler compiler;
    static const bool loaded = compiler.load();
    return loaded ? &compiler : nullptr;
}

// UTF-8 std::string -> path (a plain std::string would be read in the ANSI code page on Windows).
std::filesystem::path utf8Path(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

}  // namespace

ShaderCompileOutcome compileShaderGraph(const Graph& graph, bool wantSpirv) {
    ShaderCompileOutcome out;
    out.graph = compileGraph(graph);
    out.diagnostics = out.graph.diagnostics;
    out.success = out.graph.ok;
    // A texture file that is not there is not an error (a white texture is used, as everywhere else) but it must be visible.
    for (const TextureBinding& t : out.graph.textures) {
        if (t.path.empty() || t.path.rfind("data:", 0) == 0) continue;
        std::error_code ec;
        if (!std::filesystem::exists(utf8Path(t.path), ec))
            out.diagnostics.push_back({Severity::Warning, t.node, "texture.missing",
                "texture file not found: " + t.path + " (a white texture is used)"});
    }
    if (!out.success || !wantSpirv) return out;
    out.fragment = buildSurfaceFragment(out.graph);
    std::lock_guard<std::mutex> lock(compilerMutex());
    RuntimeCompiler* c = sharedCompiler();
    if (!c) {
        out.spirvStatus = ShaderCompileOutcome::Spirv::Unavailable;
        out.spirvError = "shader compiler unavailable (shaderc_shared not found)";
        out.diagnostics.push_back({Severity::Error, 0, "compiler.unavailable", out.spirvError});
        return out;
    }
    SpirvResult s = c->compileFragment(out.fragment.source);
    if (s.ok) {
        out.spirvStatus = ShaderCompileOutcome::Spirv::Ok;
        out.spirv = std::move(s.spirv);
        return out;
    }
    out.spirvStatus = ShaderCompileOutcome::Spirv::Failed;
    out.spirvError = s.error;
    for (int line : s.errorLines) {
        const NodeId node = nodeForFragmentLine(out.fragment, out.graph, line);
        out.diagnostics.push_back({Severity::Error, node, "glsl.error", s.error});
        break;  // the full message already lists every line
    }
    if (s.errorLines.empty()) out.diagnostics.push_back({Severity::Error, 0, "glsl.error", s.error});
    return out;
}

bool hasStructuralShaderError(const std::vector<Diagnostic>& diagnostics) {
    for (const Diagnostic& d : diagnostics) {
        if (d.severity != Severity::Error) continue;
        if (d.code == "node.id" || d.code == "node.duplicate" || d.code == "link.endpoint" ||
            d.code == "link.socket" || d.code == "link.type" || d.code == "link.multiple" || d.code == "graph.cycle")
            return true;
    }
    return false;
}

std::string shaderStructureSignature(const Graph& graph) {
    std::vector<std::string> parts;
    for (const Node& n : graph.nodes) {
        std::string s = "n" + std::to_string(n.id) + ":" + n.type + ":" + std::to_string(n.version);
        for (const auto& [k, v] : n.params)
            if (v.is_string()) s += "|" + k + "=" + v.get<std::string>();
        parts.push_back(std::move(s));
    }
    for (const Link& l : graph.links)
        parts.push_back("l" + std::to_string(l.fromNode) + "." + l.fromSocket + ">" + std::to_string(l.toNode) + "." + l.toSocket);
    std::sort(parts.begin(), parts.end());
    std::string out;
    for (const std::string& p : parts) out += p + ";";
    return out;
}

}  // namespace Phantom::ShaderGraph
