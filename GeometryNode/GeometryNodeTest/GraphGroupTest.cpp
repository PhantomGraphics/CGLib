#include "gtest/gtest.h"

#include "../GeometryNode/GeometryOps.h"
#include "../GeometryNode/GraphAsset.h"
#include "../GeometryNode/GraphEvaluator.h"
#include "../GeometryNode/GraphGroup.h"
#include "../GeometryNode/GraphJson.h"

using namespace Phantom::GeometryNode;

namespace {

NodeId add(Graph& g, const char* type) { return addNode(g, type).id; }

EvalResult run(const Graph& g, EvalCache* cache = nullptr, EvalContext ctx = EvalContext{}) {
    return evaluateGraph(g, registryFor(g), ctx, cache);
}

bool hasCode(const std::vector<Diagnostic>& d, DiagCode code) {
    for (const Diagnostic& x : d)
        if (x.code == code) return true;
    return false;
}

// Box -> Transform(Translation 4,0,0) -> Output
struct Chain {
    Graph g;
    NodeId box, tr, out;
};

Chain makeChain() {
    Chain c;
    c.box = add(c.g, "Box");
    c.tr = add(c.g, "TransformGeometry");
    c.out = add(c.g, "Output");
    setParam(*c.g.findNode(c.tr), "Translation", Vec3{4, 0, 0});
    addLink(c.g, c.box, "Geometry", c.tr, "Geometry");
    addLink(c.g, c.tr, "Geometry", c.out, "Geometry");
    return c;
}

float maxX(const EvalResult& r) { return computeBounds(*r.geometry).max.x; }

}  // namespace

TEST(GraphGroup, GroupingPreservesTheResult) {
    Chain c = makeChain();
    const EvalResult before = run(c.g);
    ASSERT_TRUE(before.success);

    std::string err;
    NodeId group = 0;
    ASSERT_TRUE(makeGroup(c.g, "Shifted", {c.tr}, &err, &group)) << err;
    ASSERT_EQ(c.g.groups.size(), 1u);
    EXPECT_EQ(c.g.groups[0].inputs.size(), 1u);   // Geometry from the Box
    EXPECT_EQ(c.g.groups[0].outputs.size(), 1u);  // Geometry to the Output
    EXPECT_EQ(c.g.findNode(group)->type, "Group:Shifted");
    EXPECT_EQ(c.g.nodes.size(), 3u);  // Box, Output, Group

    const EvalResult after = run(c.g);
    ASSERT_TRUE(after.success) << (after.diagnostics.empty() ? "" : after.diagnostics[0].message);
    EXPECT_EQ(after.geometry->positions, before.geometry->positions);
    EXPECT_EQ(after.geometry->indices, before.geometry->indices);
}

TEST(GraphGroup, GroupOfSeveralNodesAndInnerParametersTravelWithIt) {
    Chain c = makeChain();
    const NodeId second = add(c.g, "TransformGeometry");
    setParam(*c.g.findNode(second), "Translation", Vec3{0, 3, 0});
    // Box -> Transform(4,0,0) -> Transform(0,3,0) -> Output
    c.g.links.clear();
    addLink(c.g, c.box, "Geometry", c.tr, "Geometry");
    addLink(c.g, c.tr, "Geometry", second, "Geometry");
    addLink(c.g, second, "Geometry", c.out, "Geometry");
    const EvalResult before = run(c.g);
    ASSERT_TRUE(before.success);

    std::string err;
    ASSERT_TRUE(makeGroup(c.g, "Two", {c.tr, second}, &err)) << err;
    const EvalResult after = run(c.g);
    ASSERT_TRUE(after.success) << (after.diagnostics.empty() ? "" : after.diagnostics[0].message);
    EXPECT_EQ(after.geometry->positions, before.geometry->positions);
    EXPECT_NEAR(maxX(after), 4.5f, 1e-5f);
}

TEST(GraphGroup, UngroupRestoresTheOriginalStructureAndResult) {
    Chain c = makeChain();
    const EvalResult before = run(c.g);
    NodeId group = 0;
    ASSERT_TRUE(makeGroup(c.g, "G", {c.tr}, nullptr, &group));
    std::string err;
    ASSERT_TRUE(ungroup(c.g, group, &err)) << err;
    EXPECT_EQ(c.g.nodes.size(), 3u);
    EXPECT_EQ(c.g.links.size(), 2u);
    EXPECT_EQ(c.g.groups.size(), 1u);  // the definition stays in the library
    const EvalResult after = run(c.g);
    ASSERT_TRUE(after.success);
    EXPECT_EQ(after.geometry->positions, before.geometry->positions);
    bool hasTransformWithParam = false;
    for (const Node& n : c.g.nodes)
        if (n.type == "TransformGeometry") hasTransformWithParam = n.params.count("Translation") != 0;
    EXPECT_TRUE(hasTransformWithParam);

    EXPECT_TRUE(removeGroup(c.g, "G", &err)) << err;
    EXPECT_TRUE(c.g.groups.empty());
}

TEST(GraphGroup, GroupInstancesShareADefinitionAndKeepTheirOwnInputs) {
    Chain c = makeChain();
    NodeId group = 0;
    ASSERT_TRUE(makeGroup(c.g, "Shift", {c.tr}, nullptr, &group));
    // A second Box goes through another instance of the same group; both are joined.
    const NodeId box2 = add(c.g, "Box");
    setParam(*c.g.findNode(box2), "Size", Vec3{2, 2, 2});
    const NodeId second = addNode(c.g, groupNodeType("Shift")).id;
    const NodeId join = add(c.g, "JoinGeometry");
    c.g.links.clear();
    addLink(c.g, c.box, "Geometry", group, c.g.groups[0].inputs[0].id);
    addLink(c.g, box2, "Geometry", second, c.g.groups[0].inputs[0].id);
    addLink(c.g, group, c.g.groups[0].outputs[0].id, join, "Geometry");
    addLink(c.g, second, c.g.groups[0].outputs[0].id, join, "Geometry");
    addLink(c.g, join, "Geometry", c.out, "Geometry");
    const EvalResult r = run(c.g);
    ASSERT_TRUE(r.success) << (r.diagnostics.empty() ? "" : r.diagnostics[0].message);
    EXPECT_EQ(r.geometry->positions.size(), 48u);
    EXPECT_NEAR(maxX(r), 5.0f, 1e-5f);  // 4 + 1 (the 2x2x2 box)
}

TEST(GraphGroup, ScalarInterfaceInputsFlowThroughTheGroupNodeParameters) {
    Graph g;
    const NodeId box = add(g, "Box");
    const NodeId tr = add(g, "TransformGeometry");
    const NodeId combine = add(g, "CombineXYZ");
    const NodeId fl = add(g, "FloatValue");
    const NodeId out = add(g, "Output");
    setParam(*g.findNode(fl), "Value", 6.0f);
    addLink(g, box, "Geometry", tr, "Geometry");
    addLink(g, fl, "Value", combine, "X");
    addLink(g, combine, "Vector", tr, "Translation");
    addLink(g, tr, "Geometry", out, "Geometry");
    std::string err;
    NodeId group = 0;
    ASSERT_TRUE(makeGroup(g, "Move", {tr, combine}, &err, &group)) << err;
    // The Float feeding CombineXYZ.X crossed the boundary: it is an interface input of type Float.
    const GroupDef& def = g.groups[0];
    ASSERT_EQ(def.inputs.size(), 2u);
    std::string floatInput;
    for (const GroupSocket& s : def.inputs)
        if (s.type == SocketType::Float) floatInput = s.id;
    ASSERT_FALSE(floatInput.empty());
    EXPECT_NEAR(maxX(run(g)), 6.5f, 1e-5f);

    // Unlinked, the input takes the group node's parameter.
    g.links.erase(std::remove_if(g.links.begin(), g.links.end(), [&](const Link& l) { return l.to.node == group && l.to.socket == floatInput; }), g.links.end());
    setParam(*g.findNode(group), floatInput, 2.0f);
    EXPECT_NEAR(maxX(run(g)), 2.5f, 1e-5f);
}

TEST(GraphGroup, GroupGraphsRoundTripThroughJsonAndEvaluateIdentically) {
    Chain c = makeChain();
    ASSERT_TRUE(makeGroup(c.g, "Shifted", {c.tr}));
    Graph back;
    ASSERT_TRUE(parseGraph(serializeGraph(c.g), back).ok);
    ASSERT_EQ(back.groups.size(), 1u);
    EXPECT_EQ(back.groups[0].name, "Shifted");
    ASSERT_TRUE(back.groups[0].graph);
    EXPECT_EQ(back.groups[0].graph->nodes.size(), c.g.groups[0].graph->nodes.size());
    const EvalResult a = run(c.g), b = run(back);
    ASSERT_TRUE(a.success && b.success);
    EXPECT_EQ(a.geometry->positions, b.geometry->positions);

    nlohmann::json bad = graphToJson(c.g);
    bad["groups"][0]["inputs"][0]["type"] = "Nonsense";
    EXPECT_FALSE(graphFromJson(bad, back).ok);
}

TEST(GraphGroup, ErrorsInsideAGroupAreReportedOnTheGroupNode) {
    Chain c = makeChain();
    NodeId group = 0;
    ASSERT_TRUE(makeGroup(c.g, "Bad", {c.tr}, nullptr, &group));
    // Break the inside: a zero scale is singular. The definition is immutable, so rebuild it with the bad parameter.
    Graph inner = *c.g.groups[0].graph;
    for (Node& n : inner.nodes)
        if (n.type == "TransformGeometry") setParam(n, "Scale", Vec3{0, 1, 1});
    c.g.groups[0].graph = std::make_shared<const Graph>(inner);
    const EvalResult r = run(c.g);
    EXPECT_FALSE(r.success);
    bool onGroupNode = false;
    for (const Diagnostic& d : r.diagnostics)
        if (d.node == group && d.code == DiagCode::SingularTransform && d.message.find("group 'Bad'") != std::string::npos) onGroupNode = true;
    EXPECT_TRUE(onGroupNode);
}

TEST(GraphGroup, EditingTheDefinitionInvalidatesTheCacheButUnchangedGroupsAreReused) {
    Chain c = makeChain();
    NodeId group = 0;
    ASSERT_TRUE(makeGroup(c.g, "G", {c.tr}, nullptr, &group));
    EvalCache cache;
    ASSERT_TRUE(run(c.g, &cache).success);
    const EvalResult again = run(c.g, &cache);
    ASSERT_TRUE(again.success);
    EXPECT_EQ(again.stats.nodesEvaluated, 0u);  // nothing changed: everything is cached, the group included

    Graph inner = *c.g.groups[0].graph;
    for (Node& n : inner.nodes)
        if (n.type == "TransformGeometry") setParam(n, "Translation", Vec3{9, 0, 0});
    c.g.groups[0].graph = std::make_shared<const Graph>(inner);
    const EvalResult changed = run(c.g, &cache);
    ASSERT_TRUE(changed.success);
    EXPECT_NEAR(maxX(changed), 9.5f, 1e-5f);
}

TEST(GraphGroup, NestedGroupsWorkAndSelfReferenceIsCutOff) {
    Chain c = makeChain();
    NodeId inner = 0;
    ASSERT_TRUE(makeGroup(c.g, "Inner", {c.tr}, nullptr, &inner));
    NodeId outer = 0;
    ASSERT_TRUE(makeGroup(c.g, "Outer", {inner}, nullptr, &outer));
    EXPECT_EQ(c.g.groups.size(), 2u);
    const EvalResult r = run(c.g);
    ASSERT_TRUE(r.success) << (r.diagnostics.empty() ? "" : r.diagnostics[0].message);
    EXPECT_NEAR(maxX(r), 4.5f, 1e-5f);
    std::string err;
    EXPECT_FALSE(removeGroup(c.g, "Inner", &err));  // used inside "Outer"
    EXPECT_FALSE(err.empty());

    // A group whose inner graph contains itself fails with a diagnostic, never an endless recursion.
    Graph selfRef = *c.g.groups[0].graph;  // "Inner"
    Graph loop;
    const NodeId gi = add(loop, "GroupInput");
    const NodeId me = addNode(loop, groupNodeType("Inner")).id;
    const NodeId go = add(loop, "GroupOutput");
    addLink(loop, gi, c.g.groups[0].inputs[0].id, me, c.g.groups[0].inputs[0].id);
    addLink(loop, me, c.g.groups[0].outputs[0].id, go, c.g.groups[0].outputs[0].id);
    c.g.groups[0].graph = std::make_shared<const Graph>(loop);
    const EvalResult bad = run(c.g);
    EXPECT_FALSE(bad.success);
    EXPECT_TRUE(hasCode(bad.diagnostics, DiagCode::LimitExceeded));
}

TEST(GraphGroup, InvalidGroupingsAreRefusedAndChangeNothing) {
    Chain c = makeChain();
    std::string err;
    const size_t nodes = c.g.nodes.size();
    EXPECT_FALSE(makeGroup(c.g, "bad name", {c.tr}, &err));
    EXPECT_FALSE(makeGroup(c.g, "G", {}, &err));
    EXPECT_FALSE(makeGroup(c.g, "G", {c.tr, c.tr}, &err));
    EXPECT_FALSE(makeGroup(c.g, "G", {99}, &err));
    EXPECT_FALSE(makeGroup(c.g, "G", {c.out}, &err));  // the Output node cannot be grouped
    EXPECT_FALSE(makeGroup(c.g, "G", {c.box, c.tr, c.out}, &err));
    ASSERT_TRUE(addExposed(c.g, "t", c.tr, "Translation", registryFor(c.g)));
    EXPECT_FALSE(makeGroup(c.g, "G", {c.tr}, &err));  // has a public parameter
    EXPECT_EQ(c.g.nodes.size(), nodes);
    EXPECT_TRUE(c.g.groups.empty());

    // A node outside the selection between two selected nodes would make a cycle.
    Graph g;
    const NodeId a = add(g, "Box"), b = add(g, "TransformGeometry"), mid = add(g, "TransformGeometry"), out = add(g, "Output");
    addLink(g, a, "Geometry", mid, "Geometry");
    addLink(g, mid, "Geometry", b, "Geometry");
    addLink(g, b, "Geometry", out, "Geometry");
    EXPECT_FALSE(makeGroup(g, "Cyc", {a, b}, &err));
    EXPECT_NE(err.find("cycle"), std::string::npos);

    // Nothing leaves the selection: a group would produce nothing.
    Graph lonely;
    const NodeId fl = add(lonely, "FloatValue");
    add(lonely, "Output");
    EXPECT_FALSE(makeGroup(lonely, "Dead", {fl}, &err));
    EXPECT_NE(err.find("nothing leaves"), std::string::npos);
}
