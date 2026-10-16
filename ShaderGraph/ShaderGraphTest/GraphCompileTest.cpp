#include <gtest/gtest.h>

#include "../Core/GraphCompile.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

// End-to-end graph compile (GLSL, SPIR-V, diagnostics), structural classification and the structure signature, moved from
// PhantomStudio's ShaderGraphAdapter (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 4).
using namespace Phantom::ShaderGraph;

namespace {

Node mk(NodeId id, const char* type, std::map<std::string, nlohmann::json> params = {}) {
    Node n;
    n.id = id;
    n.type = type;
    n.params = std::move(params);
    return n;
}

Link lk(NodeId a, const char* as, NodeId b, const char* bs) { return {a, as, b, bs}; }

// SurfaceOutput(BaseColor = Color * tint) fed by one texture, like a material with a base colour texture.
Graph texturedGraph(const std::string& path) {
    Graph g;
    g.nodes = {mk(1, "SurfaceOutput", {{"Metallic", 0.0}, {"Roughness", 0.5}}),
               mk(2, "ImageTexture", {{"Path", path}, {"ColorSpace", "sRGB"}}),
               mk(3, "Multiply", {{"Type", "Color"}, {"B", {0.8, 0.8, 0.8}}})};
    g.links = {lk(2, "Color", 3, "A"), lk(3, "Result", 1, "BaseColor")};
    return g;
}

bool hasCode(const std::vector<Diagnostic>& d, const char* code) {
    for (const auto& x : d) if (x.code == code) return true;
    return false;
}

}  // namespace

TEST(GraphCompile, GlslOnlyDoesNotNeedTheRuntimeCompiler) {
    const ShaderCompileOutcome r = compileShaderGraph(texturedGraph("data:image/png;base64,AAAA"), false);
    ASSERT_TRUE(r.success);
    EXPECT_EQ(ShaderCompileOutcome::Spirv::NotRequested, r.spirvStatus);
    EXPECT_FALSE(r.graph.glsl.empty());
    EXPECT_TRUE(r.spirv.empty());
    EXPECT_TRUE(r.fragment.source.empty());
    EXPECT_FALSE(hasCode(r.diagnostics, "texture.missing"));   // data: URIs are not files
}

TEST(GraphCompile, MissingTextureFileIsAWarningNotAnError) {
    const std::string missing = (std::filesystem::temp_directory_path() / "phantom_graphcompile_missing.png").string();
    std::filesystem::remove(missing);
    const ShaderCompileOutcome r = compileShaderGraph(texturedGraph(missing), false);
    EXPECT_TRUE(r.success);
    ASSERT_TRUE(hasCode(r.diagnostics, "texture.missing"));
    for (const auto& d : r.diagnostics)
        if (d.code == "texture.missing") { EXPECT_EQ(Severity::Warning, d.severity); EXPECT_EQ(2, d.node); }

    // An existing file (any content: only existence is checked here) gives no warning.
    const std::string present = (std::filesystem::temp_directory_path() / "phantom_graphcompile_present.png").string();
    { std::ofstream out(present, std::ios::binary); out << "x"; }
    EXPECT_FALSE(hasCode(compileShaderGraph(texturedGraph(present), false).diagnostics, "texture.missing"));
    std::filesystem::remove(present);
}

TEST(GraphCompile, InvalidGraphHasNoFragmentAndKeepsItsDiagnostics) {
    Graph g = texturedGraph("");
    g.links.push_back(lk(3, "Result", 3, "A"));   // a cycle
    const ShaderCompileOutcome r = compileShaderGraph(g, true);
    EXPECT_FALSE(r.success);
    EXPECT_EQ(ShaderCompileOutcome::Spirv::NotRequested, r.spirvStatus);
    EXPECT_TRUE(r.fragment.source.empty());
    EXPECT_TRUE(hasStructuralShaderError(r.diagnostics));
}

TEST(GraphCompile, SpirvIsProducedWhenTheCompilerIsAvailableAndReportedUnavailableOtherwise) {
    const ShaderCompileOutcome r = compileShaderGraph(texturedGraph("data:image/png;base64,AAAA"), true);
    ASSERT_TRUE(r.success);
    EXPECT_FALSE(r.fragment.source.empty());
    if (r.spirvStatus == ShaderCompileOutcome::Spirv::Ok) {
        ASSERT_FALSE(r.spirv.empty());
        EXPECT_EQ(0x07230203u, r.spirv[0]);
        EXPECT_TRUE(r.spirvError.empty());
    } else {
        // shaderc_shared is not installed on this machine: GLSL is still produced and the reason is a diagnostic.
        EXPECT_EQ(ShaderCompileOutcome::Spirv::Unavailable, r.spirvStatus);
        EXPECT_TRUE(r.spirv.empty());
        EXPECT_TRUE(hasCode(r.diagnostics, "compiler.unavailable"));
    }
}

TEST(GraphCompile, StructuralErrorsAreSeparatedFromOrdinaryDiagnostics) {
    EXPECT_FALSE(hasStructuralShaderError({}));
    EXPECT_FALSE(hasStructuralShaderError({{Severity::Warning, 1, "link.type", "ignored: only errors count"}}));
    EXPECT_FALSE(hasStructuralShaderError({{Severity::Error, 0, "output.missing", "no output node"},
                                           {Severity::Error, 0, "glsl.error", "compile failure"}}));
    for (const char* code : {"node.id", "node.duplicate", "link.endpoint", "link.socket", "link.type", "link.multiple", "graph.cycle"})
        EXPECT_TRUE(hasStructuralShaderError({{Severity::Error, 1, code, "x"}})) << code;
}

TEST(GraphCompile, StructureSignatureIgnoresValuesAndLayout) {
    Graph g = texturedGraph("a.png");
    const std::string base = shaderStructureSignature(g);
    g.findNode(1)->params["Roughness"] = 0.9;
    g.findNode(3)->params["B"] = {0.1, 0.2, 0.3};
    g.layout[1] = {5, 5};
    EXPECT_EQ(base, shaderStructureSignature(g));

    // A string parameter (Path / ColorSpace / Type), a new node or a new link is a structure change.
    Graph changedPath = g;
    changedPath.findNode(2)->params["Path"] = "b.png";
    EXPECT_NE(base, shaderStructureSignature(changedPath));
    Graph extraNode = g;
    extraNode.nodes.push_back(mk(9, "Float"));
    EXPECT_NE(base, shaderStructureSignature(extraNode));
    Graph extraLink = g;
    extraLink.links.push_back(lk(9, "Value", 1, "Metallic"));
    EXPECT_NE(base, shaderStructureSignature(extraLink));

    // The signature does not depend on node or link order.
    Graph reordered = g;
    std::reverse(reordered.nodes.begin(), reordered.nodes.end());
    std::reverse(reordered.links.begin(), reordered.links.end());
    EXPECT_EQ(base, shaderStructureSignature(reordered));
}
