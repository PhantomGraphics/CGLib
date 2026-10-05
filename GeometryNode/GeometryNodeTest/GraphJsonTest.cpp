#include "gtest/gtest.h"

#include "../GeometryNode/GraphEvaluator.h"
#include "../GeometryNode/GraphJson.h"

using namespace Phantom::GeometryNode;

namespace {

Graph sampleGraph() {
    Graph g;
    NodeId box = addNode(g, "Box").id;
    NodeId tr = addNode(g, "TransformGeometry").id;
    NodeId out = addNode(g, "Output").id;
    setParam(*g.findNode(box), "Size", Vec3{1, 2, 3});
    setParam(*g.findNode(tr), "Translation", Vec3{4, 5, 6});
    addLink(g, box, "Geometry", tr, "Geometry");
    addLink(g, tr, "Geometry", out, "Geometry");
    g.layout[box] = NodeLayout{-200.5, 10};
    g.layout[tr] = NodeLayout{0, 10};
    g.layout[out] = NodeLayout{200, 10};
    return g;
}

}  // namespace

TEST(GraphJson, RoundTripPreservesEverything) {
    const Graph g = sampleGraph();
    const std::string text = serializeGraph(g);
    Graph back;
    const GraphParseResult r = parseGraph(text, back);
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(serializeGraph(back), text);  // byte-stable
    EXPECT_EQ(back.nodes.size(), 3u);
    EXPECT_EQ(back.links.size(), 2u);
    EXPECT_EQ(back.layout.at(1).x, -200.5);

    // And it still evaluates identically.
    const EvalResult a = evaluateGraph(g, NodeRegistry::builtin(), EvalContext{});
    const EvalResult b = evaluateGraph(back, NodeRegistry::builtin(), EvalContext{});
    ASSERT_TRUE(a.success && b.success);
    EXPECT_EQ(a.geometry->positions, b.geometry->positions);
}

TEST(GraphJson, ComputationAndLayoutAreSeparate) {
    Graph g = sampleGraph();
    const nlohmann::json j = graphToJson(g);
    EXPECT_TRUE(j.contains("layout"));
    EXPECT_FALSE(j["nodes"][0].contains("x"));  // no editor data inside nodes
    EXPECT_EQ(j["schema"], 1);
}

TEST(GraphJson, UnknownNodesParametersAndKeysSurvive) {
    const char* text = R"({
        "schema": 1,
        "futureRoot": {"a": [1, 2]},
        "nodes": [
            {"id": 1, "type": "HologramNode", "version": 7, "params": {"Glow": [1, 2, 3], "Mode": "x"}, "note": "hi"},
            {"id": 2, "type": "Box", "params": {"Size": [1, 1, 1], "Unknown": true}},
            {"id": 3, "type": "Output"}
        ],
        "links": []
    })";
    Graph g;
    ASSERT_TRUE(parseGraph(text, g).ok);
    ASSERT_EQ(g.nodes.size(), 3u);  // the unknown node is not dropped on load
    Graph again;
    ASSERT_TRUE(parseGraph(serializeGraph(g), again).ok);
    EXPECT_EQ(again.nodes[0].type, "HologramNode");
    EXPECT_EQ(again.nodes[0].version, 7u);
    EXPECT_EQ(again.nodes[0].params.at("Glow"), nlohmann::json::array({1, 2, 3}));
    EXPECT_EQ(again.nodes[0].extra["note"], "hi");
    EXPECT_EQ(again.nodes[1].params.at("Unknown"), true);
    EXPECT_EQ(again.extra["futureRoot"]["a"][1], 2);

    // Evaluation reports the unknown node instead of silently succeeding.
    addLink(again, 1, "Geometry", 3, "Geometry");
    auto diags = validateGraph(again, NodeRegistry::builtin());
    bool unknown = false;
    for (const Diagnostic& d : diags) unknown = unknown || d.code == DiagCode::UnknownNodeType;
    EXPECT_TRUE(unknown);
}

TEST(GraphJson, RejectsBadInputWithoutTouchingOutput) {
    Graph g = sampleGraph();
    const std::string before = serializeGraph(g);

    EXPECT_FALSE(parseGraph("not json", g).ok);
    EXPECT_FALSE(parseGraph("[]", g).ok);
    EXPECT_FALSE(parseGraph(R"({"nodes": []})", g).ok);                         // no schema
    EXPECT_FALSE(parseGraph(R"({"schema": 999, "nodes": []})", g).ok);          // newer than supported
    EXPECT_FALSE(parseGraph(R"({"schema": 1, "nodes": [{"type": "Box"}]})", g).ok);  // no id
    EXPECT_FALSE(parseGraph(R"({"schema": 1, "links": [{"from": {}, "to": {}}]})", g).ok);
    EXPECT_FALSE(parseGraph(R"({"schema": 1, "layout": {"nodes": {"abc": {"x": 0, "y": 0}}}})", g).ok);

    EXPECT_EQ(serializeGraph(g), before);
}
