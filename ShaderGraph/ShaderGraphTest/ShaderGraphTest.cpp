#include <gtest/gtest.h>

#include "../ShaderGraph/ShaderGraph.h"

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

// UV -> ImageTexture -> Multiply(Color) -> SurfaceOutput.BaseColor  (the plan's first example)
Graph exampleGraph() {
    Graph g;
    g.nodes = {mk(1, "UV"),
               mk(2, "ImageTexture", {{"Path", "checker.png"}}),
               mk(3, "Multiply", {{"Type", "Color"}, {"B", {1.0, 0.5, 0.25}}}),
               mk(4, "SurfaceOutput", {{"Roughness", 0.3}})};
    g.links = {lk(1, "UV", 2, "UV"), lk(2, "Color", 3, "A"), lk(3, "Result", 4, "BaseColor")};
    return g;
}

bool hasCode(const std::vector<Diagnostic>& d, const char* code) {
    for (const auto& x : d) if (x.code == code) return true;
    return false;
}

}  // namespace

TEST(ShaderGraphCompile, ExampleGeneratesGlsl) {
    const CompileResult r = compileGraph(exampleGraph());
    ASSERT_TRUE(r.ok) << (r.diagnostics.empty() ? "" : r.diagnostics[0].message);
    EXPECT_NE(r.glsl.find("texture(sg_tex0, n1_UV)"), std::string::npos) << r.glsl;
    EXPECT_NE(r.glsl.find("n3_Result = n2_Color * sg_p["), std::string::npos) << r.glsl;
    EXPECT_NE(r.glsl.find("s.baseColor = n3_Result;"), std::string::npos);
    ASSERT_EQ(r.textures.size(), 1u);
    EXPECT_EQ(r.textures[0].path, "checker.png");
    EXPECT_TRUE(r.textures[0].srgb);
}

TEST(ShaderGraphCompile, Deterministic) {
    EXPECT_EQ(compileGraph(exampleGraph()).glsl, compileGraph(exampleGraph()).glsl);
    EXPECT_EQ(compileGraph(exampleGraph()).cacheKey, compileGraph(exampleGraph()).cacheKey);
}

TEST(ShaderGraphCompile, ValueEditKeepsCacheKeyAndRepacks) {
    Graph g = exampleGraph();
    const CompileResult a = compileGraph(g);
    g.findNode(4)->params["Roughness"] = 0.9;
    g.findNode(3)->params["B"] = {0.1, 0.2, 0.3};
    const CompileResult b = compileGraph(g);
    EXPECT_EQ(a.cacheKey, b.cacheKey);
    const std::vector<float> packed = packParameters(g, b);
    bool found = false;
    for (const ParamSlot& p : b.params) {
        if (p.node == 4 && p.name == "Roughness") {
            EXPECT_FLOAT_EQ(packed[p.slot * 4], 0.9f);
            found = true;
        }
        if (p.node == 3 && p.name == "B") EXPECT_FLOAT_EQ(packed[p.slot * 4 + 2], 0.3f);
    }
    EXPECT_TRUE(found);
}

TEST(ShaderGraphCompile, LayoutDoesNotAffectOutput) {
    Graph g = exampleGraph();
    const std::string a = compileGraph(g).glsl;
    g.layout[1] = {100, 200};
    EXPECT_EQ(a, compileGraph(g).glsl);
}

TEST(ShaderGraphCompile, UnreachableNodesNotEmitted) {
    Graph g = exampleGraph();
    g.nodes.push_back(mk(9, "Float", {{"Value", 3.0}}));
    const CompileResult r = compileGraph(g);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.glsl.find("n9_"), std::string::npos);
}

TEST(ShaderGraphCompile, SharedTextureDeduplicated) {
    Graph g = exampleGraph();
    g.nodes.push_back(mk(5, "ImageTexture", {{"Path", "checker.png"}}));
    g.nodes.push_back(mk(6, "Mix", {{"Type", "Color"}}));
    g.links.clear();
    g.links = {lk(2, "Color", 6, "A"), lk(5, "Color", 6, "B"), lk(6, "Result", 4, "BaseColor")};
    const CompileResult r = compileGraph(g);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.textures.size(), 1u);
}

TEST(ShaderGraphCompile, NormalMapAndLinearTexture) {
    Graph g;
    g.nodes = {mk(1, "ImageTexture", {{"Path", "n.png"}, {"ColorSpace", "Linear"}}), mk(2, "NormalMap"),
               mk(3, "SurfaceOutput")};
    g.links = {lk(1, "Color", 2, "Color"), lk(2, "Normal", 3, "Normal")};
    const CompileResult r = compileGraph(g);
    ASSERT_TRUE(r.ok);
    EXPECT_FALSE(r.textures[0].srgb);
    EXPECT_NE(r.glsl.find("s.normal = n2_Normal;"), std::string::npos);
}

TEST(ShaderGraphCompile, UnconnectedNormalUsesGeometricNormal) {
    Graph g;
    g.nodes = {mk(1, "SurfaceOutput")};
    const CompileResult r = compileGraph(g);
    ASSERT_TRUE(r.ok);
    EXPECT_NE(r.glsl.find("s.normal = tbn[2];"), std::string::npos);
}

TEST(ShaderGraphCompile, LineMapPointsBackToNodes) {
    const CompileResult r = compileGraph(exampleGraph());
    ASSERT_TRUE(r.ok);
    for (const LineRange& lr : r.lineMap) {
        EXPECT_EQ(nodeForGlslLine(r, lr.firstLine), lr.node);
    }
    EXPECT_EQ(nodeForGlslLine(r, 1), 0);
}

TEST(ShaderGraphCompile, ProceduralNodesEmitOnlyWhatTheyNeed) {
    Graph g;
    g.nodes = {mk(1, "Checker", {{"Scale", 8.0}}), mk(2, "SurfaceOutput")};
    g.links = {lk(1, "Color", 2, "BaseColor")};
    CompileResult r = compileGraph(g);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.glsl.find("sg_fbm"), std::string::npos) << "noise helpers only when a Noise node is reachable";
    EXPECT_NE(r.glsl.find("n1_Fac = mod("), std::string::npos) << r.glsl;
    EXPECT_NE(r.glsl.find("floor(uv * sg_p["), std::string::npos) << "unconnected UV falls back to the mesh uv";

    g.nodes.push_back(mk(3, "Noise"));
    g.nodes.push_back(mk(4, "Fresnel"));
    g.links = {lk(1, "Color", 2, "BaseColor"), lk(3, "Fac", 2, "Roughness"), lk(4, "Fac", 2, "Metallic")};
    r = compileGraph(g);
    ASSERT_TRUE(r.ok);
    EXPECT_NE(r.glsl.find("float sg_fbm("), std::string::npos);
    EXPECT_NE(r.glsl.find("cam.camPos - fragPos"), std::string::npos) << "Fresnel uses the view vector";
    EXPECT_NE(r.glsl.find("tbn[2]"), std::string::npos) << "unconnected Normal is the geometric normal";

    // Values are UBO slots: editing them keeps the pipeline.
    const uint64_t key = r.cacheKey;
    g.findNode(3)->params["Scale"] = 20.0;
    g.findNode(4)->params["IOR"] = 2.0;
    EXPECT_EQ(compileGraph(g).cacheKey, key);
}

TEST(ShaderGraphValidate, ProceduralNodeTypeChecks) {
    Graph g;
    g.nodes = {mk(1, "Checker"), mk(2, "Noise"), mk(3, "SurfaceOutput")};
    g.links = {lk(1, "Fac", 3, "BaseColor")};  // Float -> Color is never implicit
    EXPECT_TRUE(hasCode(validateGraph(g), "link.type"));
}

TEST(ShaderGraphValidate, Cycle) {
    Graph g;
    g.nodes = {mk(1, "Add"), mk(2, "Add"), mk(3, "SurfaceOutput")};
    g.links = {lk(1, "Result", 2, "A"), lk(2, "Result", 1, "A")};
    EXPECT_TRUE(hasCode(validateGraph(g), "graph.cycle"));
    EXPECT_FALSE(compileGraph(g).ok);
}

TEST(ShaderGraphValidate, TypeMismatchNeverImplicit) {
    Graph g;
    g.nodes = {mk(1, "Vector"), mk(2, "SurfaceOutput")};
    g.links = {lk(1, "Vector", 2, "BaseColor")};  // Vec3 -> Color
    EXPECT_TRUE(hasCode(validateGraph(g), "link.type"));
    g.nodes = {mk(1, "Color"), mk(2, "SurfaceOutput")};
    g.links = {lk(1, "Color", 2, "Normal")};  // Color -> Normal
    EXPECT_TRUE(hasCode(validateGraph(g), "link.type"));
}

TEST(ShaderGraphValidate, StructuralErrors) {
    Graph g;
    g.nodes = {mk(1, "Float"), mk(1, "Float"), mk(2, "SurfaceOutput"), mk(3, "SurfaceOutput")};
    g.links = {lk(1, "Value", 2, "Metallic"), lk(1, "Value", 2, "Metallic"), lk(77, "Value", 2, "Roughness"),
               lk(1, "Nope", 2, "Roughness")};
    const auto d = validateGraph(g);
    EXPECT_TRUE(hasCode(d, "node.duplicate"));
    EXPECT_TRUE(hasCode(d, "output.multiple"));
    EXPECT_TRUE(hasCode(d, "link.multiple"));
    EXPECT_TRUE(hasCode(d, "link.endpoint"));
    EXPECT_TRUE(hasCode(d, "link.socket"));
}

TEST(ShaderGraphValidate, MissingOutput) {
    Graph g;
    g.nodes = {mk(1, "Float")};
    EXPECT_TRUE(hasCode(validateGraph(g), "output.missing"));
}

TEST(ShaderGraphValidate, UnknownNodeOnlyErrorWhenReachable) {
    Graph g;
    g.nodes = {mk(1, "Frobnicate"), mk(2, "SurfaceOutput")};
    auto d = validateGraph(g);
    ASSERT_TRUE(hasCode(d, "node.unknown"));
    EXPECT_FALSE(hasErrors(d));  // unused: warning, graph still compiles
    EXPECT_TRUE(compileGraph(g).ok);
    g.links = {lk(1, "Out", 2, "BaseColor")};
    EXPECT_TRUE(hasErrors(validateGraph(g)));
}

TEST(ShaderGraphValidate, BadParams) {
    Graph g;
    g.nodes = {mk(1, "Add", {{"Type", "Normal"}}), mk(2, "Float", {{"Value", "x"}}),
               mk(3, "ImageTexture", {{"ColorSpace", "HDR"}}), mk(4, "SurfaceOutput")};
    const auto d = validateGraph(g);
    int n = 0;
    for (const auto& x : d) n += x.node == 1 || x.node == 2 || x.node == 3;
    EXPECT_EQ(n, 3);
}

TEST(ShaderGraphValidate, TextureLimit) {
    Graph g;
    g.nodes.push_back(mk(100, "SurfaceOutput"));
    for (int i = 1; i <= kMaxTextures + 1; ++i) g.nodes.push_back(mk(i, "ImageTexture", {{"Path", "a.png"}}));
    // chain them through Add so all are reachable
    NodeId prev = 1;
    for (int i = 2; i <= kMaxTextures + 1; ++i) {
        const NodeId add = 200 + i;
        g.nodes.push_back(mk(add, "Add", {{"Type", "Color"}}));
        g.links.push_back(lk(prev, prev < 200 ? "Color" : "Result", add, "A"));
        g.links.push_back(lk(i, "Color", add, "B"));
        prev = add;
    }
    g.links.push_back(lk(prev, "Result", 100, "BaseColor"));
    EXPECT_TRUE(hasCode(validateGraph(g), "limit.textures"));
}

TEST(ShaderGraphAsset, ExposeOnlyUnlinkedValues) {
    Graph g = exampleGraph();  // 2 = ImageTexture(checker.png), 3 = Multiply(Color), 4 = SurfaceOutput
    std::string err;
    EXPECT_TRUE(addExposed(g, "tint", 3, "B", &err)) << err;
    EXPECT_TRUE(addExposed(g, "roughness", 4, "Roughness", &err)) << err;
    EXPECT_TRUE(addExposed(g, "albedo-map", 2, "Path", &err)) << err;

    EXPECT_FALSE(addExposed(g, "tint", 4, "Metallic", &err)) << "names are unique";
    EXPECT_FALSE(addExposed(g, "bad name", 4, "Metallic", &err)) << "names use letters, digits, _ and -";
    EXPECT_FALSE(addExposed(g, "a", 3, "A", &err)) << "a linked input holds no value of its own";
    EXPECT_FALSE(addExposed(g, "b", 3, "Type", &err)) << "Type is structural, not a value";
    EXPECT_FALSE(addExposed(g, "c", 2, "UV", &err)) << "an input with a builtin source has no value";
    EXPECT_FALSE(addExposed(g, "d", 4, "Normal", &err));
    EXPECT_FALSE(addExposed(g, "e", 99, "Value", &err)) << "unknown node";
    EXPECT_EQ(g.exposed.size(), 3u) << "a refused exposure leaves the graph untouched";

    const auto info = describeExposed(g);
    ASSERT_EQ(info.size(), 3u);
    EXPECT_TRUE(info[0].valid);
    EXPECT_EQ(info[0].type, SocketType::Color);
    EXPECT_EQ(info[0].value, nlohmann::json::array({1.0, 0.5, 0.25}));
    EXPECT_NEAR(info[1].value.get<double>(), 0.3, 1e-6);  // the parameter, not the default
    EXPECT_TRUE(info[2].isPath);
    EXPECT_EQ(info[2].value, "checker.png");

    EXPECT_TRUE(removeExposed(g, "tint"));
    EXPECT_FALSE(removeExposed(g, "tint"));
    EXPECT_EQ(g.exposed.size(), 2u);
}

TEST(ShaderGraphAsset, DefaultValueIsReportedWhenNoParameterIsSet) {
    Graph g = exampleGraph();
    ASSERT_TRUE(addExposed(g, "metal", 4, "Metallic"));  // not set on the node: the socket default
    EXPECT_FALSE(addExposed(g, "base", 4, "BaseColor")) << "BaseColor is linked in the example";
    const auto info = describeExposed(g);
    ASSERT_EQ(info.size(), 1u);
    EXPECT_EQ(info[0].value.get<double>(), 0.0);
}

TEST(ShaderGraphAsset, OverridesKeepTheStructureAndTheCacheKey) {
    Graph asset = exampleGraph();
    ASSERT_TRUE(addExposed(asset, "tint", 3, "B"));
    ASSERT_TRUE(addExposed(asset, "roughness", 4, "Roughness"));
    const CompileResult base = compileGraph(asset);
    ASSERT_TRUE(base.ok);

    Graph effective;
    std::string err;
    ASSERT_TRUE(applyOverrides(asset, {{"tint", {0.1, 0.2, 0.3}}, {"roughness", 0.9}}, effective, &err)) << err;
    const CompileResult c = compileGraph(effective);
    ASSERT_TRUE(c.ok);
    EXPECT_EQ(c.cacheKey, base.cacheKey) << "value overrides never change the generated code";
    const std::vector<float> packed = packParameters(effective, c);
    bool sawTint = false, sawRoughness = false;
    for (const ParamSlot& p : c.params) {
        if (p.node == 3 && p.name == "B") { sawTint = true; EXPECT_FLOAT_EQ(packed[p.slot * 4 + 1], 0.2f); }
        if (p.node == 4 && p.name == "Roughness") { sawRoughness = true; EXPECT_FLOAT_EQ(packed[p.slot * 4], 0.9f); }
    }
    EXPECT_TRUE(sawTint && sawRoughness);
    EXPECT_EQ(asset.findNode(4)->params["Roughness"], 0.3) << "the asset itself is never modified";
    EXPECT_EQ(effective.exposed.size(), 2u) << "the effective graph still describes its exposed parameters";
}

TEST(ShaderGraphAsset, PathOverrideSwapsTheTexture) {
    Graph asset = exampleGraph();
    ASSERT_TRUE(addExposed(asset, "map", 2, "Path"));
    Graph effective;
    ASSERT_TRUE(applyOverrides(asset, {{"map", "bricks.png"}}, effective));
    const CompileResult c = compileGraph(effective);
    ASSERT_TRUE(c.ok);
    ASSERT_EQ(c.textures.size(), 1u);
    EXPECT_EQ(c.textures[0].path, "bricks.png");
    EXPECT_EQ(c.cacheKey, compileGraph(asset).cacheKey) << "same bindings, so the same pipeline";
}

TEST(ShaderGraphAsset, BadOverridesAreRefusedAndLeaveTheOutputAlone) {
    Graph asset = exampleGraph();
    ASSERT_TRUE(addExposed(asset, "tint", 3, "B"));
    ASSERT_TRUE(addExposed(asset, "roughness", 4, "Roughness"));
    ASSERT_TRUE(addExposed(asset, "map", 2, "Path"));
    Graph out = exampleGraph();
    out.extra["marker"] = 1;
    std::string err;
    EXPECT_FALSE(applyOverrides(asset, {{"nope", 1.0}}, out, &err));
    EXPECT_NE(err.find("nope"), std::string::npos);
    EXPECT_FALSE(applyOverrides(asset, {{"roughness", "rough"}}, out, &err)) << "a string for a Float";
    EXPECT_FALSE(applyOverrides(asset, {{"tint", 0.5}}, out, &err)) << "a scalar for a Color";
    EXPECT_FALSE(applyOverrides(asset, {{"tint", {1, 2}}}, out, &err)) << "too few components";
    EXPECT_FALSE(applyOverrides(asset, {{"map", 3}}, out, &err)) << "a number for a path";
    EXPECT_FALSE(applyOverrides(asset, {{"roughness", 0.5}, {"nope", 1}}, out, &err)) << "all or nothing";
    EXPECT_EQ(out.extra.value("marker", 0), 1);

    // A parameter that stopped qualifying (its input got linked) cannot be overridden.
    Graph stale = asset;
    stale.nodes.push_back(mk(9, "Float"));
    stale.links.push_back(lk(9, "Value", 4, "Roughness"));
    EXPECT_FALSE(applyOverrides(stale, {{"roughness", 0.5}}, out, &err));
}

TEST(ShaderGraphAsset, StaleExposureIsAWarningNotAnError) {
    Graph g = exampleGraph();
    ASSERT_TRUE(addExposed(g, "roughness", 4, "Roughness"));
    g.nodes.push_back(mk(9, "Float"));
    g.links.push_back(lk(9, "Value", 4, "Roughness"));  // the exposed input is now driven by a node
    auto d = validateGraph(g);
    EXPECT_TRUE(hasCode(d, "expose.invalid"));
    EXPECT_FALSE(hasErrors(d));
    EXPECT_TRUE(compileGraph(g).ok);
    EXPECT_FALSE(describeExposed(g)[0].valid);

    g.exposed.push_back({"roughness", 4, "Metallic"});
    EXPECT_TRUE(hasCode(validateGraph(g), "expose.duplicate"));
}

TEST(ShaderGraphJson, ExposedParametersRoundTrip) {
    Graph g = exampleGraph();
    ASSERT_TRUE(addExposed(g, "tint", 3, "B"));
    ASSERT_TRUE(addExposed(g, "map", 2, "Path"));
    g.extra["note"] = "keep";
    Graph back;
    const ParseResult r = parseGraph(serializeGraph(g), back);
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_EQ(back.exposed.size(), 2u);
    EXPECT_EQ(back.exposed[0].name, "tint");
    EXPECT_EQ(back.exposed[0].node, 3);
    EXPECT_EQ(back.exposed[0].param, "B");
    EXPECT_EQ(back.exposed[1].param, "Path");
    EXPECT_EQ(back.extra["note"], "keep");
    EXPECT_EQ(graphToJson(back), graphToJson(g));
    EXPECT_FALSE(graphToJson(exampleGraph()).contains("exposed")) << "graphs without exposure write nothing new";

    EXPECT_FALSE(parseGraph(R"({"exposed":[{"name":"x","node":1}]})", back).ok);
    EXPECT_FALSE(parseGraph(R"({"exposed":{"name":"x"}})", back).ok);
    EXPECT_EQ(back.exposed.size(), 2u) << "a rejected parse leaves the output untouched";
}

TEST(ShaderGraphJson, RoundTripPreservesEverything) {
    Graph g = exampleGraph();
    g.layout[2] = {10.5, -3};
    g.extra["note"] = "hello";
    g.nodes[0].extra["editorColor"] = 7;
    g.nodes.push_back(mk(8, "FutureNode", {{"Mystery", {1, 2}}}));
    const std::string text = serializeGraph(g);
    Graph back;
    const ParseResult pr = parseGraph(text, back);
    ASSERT_TRUE(pr.ok) << pr.error;
    EXPECT_EQ(serializeGraph(back), text);
    EXPECT_EQ(back.extra["note"], "hello");
    EXPECT_EQ(back.nodes[0].extra["editorColor"], 7);
    EXPECT_EQ(back.nodes.back().type, "FutureNode");
    EXPECT_DOUBLE_EQ(back.layout[2].x, 10.5);
    EXPECT_EQ(compileGraph(back).glsl, compileGraph(g).glsl);
}

TEST(ShaderGraphJson, RejectsBadInput) {
    Graph g = exampleGraph();
    const size_t before = g.nodes.size();
    EXPECT_FALSE(parseGraph("{not json", g).ok);
    EXPECT_FALSE(parseGraph(R"({"schema":999})", g).ok);
    EXPECT_FALSE(parseGraph(R"({"nodes":[{"id":1}]})", g).ok);
    EXPECT_FALSE(parseGraph(R"({"links":[{"from":1,"to":2}]})", g).ok);
    EXPECT_EQ(g.nodes.size(), before);  // untouched on failure
}

// ---- Phase 0: runtime compile + mesh surface fragment ABI ------------------------

#include "../ShaderGraph/RuntimeCompiler.h"

namespace {

// shaderc_shared is optional on the machine running the tests.
#define REQUIRE_COMPILER(c)                                       \
    RuntimeCompiler c;                                            \
    if (!c.load()) GTEST_SKIP() << "shaderc_shared: " << c.loadError()

}  // namespace

TEST(ShaderGraphRuntime, ExampleCompilesToSpirv) {
    REQUIRE_COMPILER(c);
    const CompileResult g = compileGraph(exampleGraph());
    ASSERT_TRUE(g.ok);
    const SurfaceFragment f = buildSurfaceFragment(g);
    const SpirvResult s = c.compileFragment(f.source);
    ASSERT_TRUE(s.ok) << s.error << "\n" << f.source;
    EXPECT_EQ(s.spirv[0], 0x07230203u);
}

TEST(ShaderGraphRuntime, EveryNodeTypeCompiles) {
    REQUIRE_COMPILER(c);
    Graph g;
    g.nodes = {mk(1, "UV"), mk(2, "ImageTexture", {{"Path", "a.png"}}), mk(3, "Multiply", {{"Type", "Color"}}),
               mk(4, "ImageTexture", {{"Path", "n.png"}, {"ColorSpace", "Linear"}}), mk(5, "NormalMap"),
               mk(6, "Split"), mk(7, "Clamp"), mk(8, "Combine"), mk(9, "ToColor"), mk(10, "Mix", {{"Type", "Vec2"}}),
               mk(11, "SurfaceOutput"), mk(12, "Vector", {{"Vector", {1, 2, 3}}}), mk(13, "Float", {{"Value", 0.7}}),
               mk(14, "Add", {{"Type", "Float"}}), mk(15, "Color"), mk(16, "ToVector"), mk(17, "Add", {{"Type", "Vec2"}}),
               mk(18, "Split"), mk(19, "Checker"), mk(20, "Noise"), mk(21, "Fresnel")};
    g.links = {lk(1, "UV", 2, "UV"), lk(2, "Color", 3, "A"), lk(3, "Result", 11, "BaseColor"),
               lk(4, "Color", 5, "Color"), lk(5, "Normal", 11, "Normal"), lk(12, "Vector", 6, "Vector"),
               lk(6, "X", 7, "Value"), lk(7, "Result", 14, "A"), lk(13, "Value", 14, "B"),
               lk(14, "Result", 11, "Roughness"), lk(6, "Y", 8, "X"), lk(8, "Vector", 9, "Vector"),
               lk(9, "Color", 3, "B"), lk(15, "Color", 16, "Color"), lk(16, "Vector", 18, "Vector"),
               lk(18, "Z", 11, "Metallic"), lk(1, "UV", 17, "A"), lk(17, "Result", 10, "A"), lk(10, "Result", 2, "UV"),
               lk(19, "Fac", 7, "Min"), lk(20, "Fac", 8, "Z"), lk(21, "Fac", 7, "Max")};
    // 10/17 feed the first texture's UV -> exercise Vec2 paths; (1,UV)->(2,UV) was replaced below
    g.links.erase(g.links.begin());
    const CompileResult r = compileGraph(g);
    ASSERT_TRUE(r.ok) << (r.diagnostics.empty() ? "" : r.diagnostics[0].message);
    const SpirvResult s = c.compileFragment(buildSurfaceFragment(r).source);
    EXPECT_TRUE(s.ok) << s.error;
}

TEST(ShaderGraphRuntime, FailureReportsErrorLineAndNode) {
    REQUIRE_COMPILER(c);
    CompileResult g = compileGraph(exampleGraph());
    ASSERT_TRUE(g.ok);
    // Corrupt the Multiply node's line only.
    const size_t at = g.glsl.find("n3_Result = ");
    ASSERT_NE(at, std::string::npos);
    g.glsl.replace(at, 12, "n3_Result = undefined_symbol + ");
    const SurfaceFragment f = buildSurfaceFragment(g);
    const SpirvResult s = c.compileFragment(f.source);
    ASSERT_FALSE(s.ok);
    ASSERT_FALSE(s.errorLines.empty()) << s.error;
    EXPECT_EQ(nodeForFragmentLine(f, g, s.errorLines[0]), 3) << s.error;
}

TEST(ShaderGraphRuntime, UnloadedCompilerFailsCleanly) {
    RuntimeCompiler none;
    const SpirvResult s = none.compileFragment("void main(){}");
    EXPECT_FALSE(s.ok);
    EXPECT_FALSE(s.error.empty());
}
