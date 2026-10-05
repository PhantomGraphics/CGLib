#include "gtest/gtest.h"

#include <atomic>

#include "../GeometryNode/GeometryOps.h"
#include "../GeometryNode/GraphEvaluator.h"

using namespace Phantom::GeometryNode;

namespace {

const NodeRegistry& reg() { return NodeRegistry::builtin(); }

bool hasCode(const std::vector<Diagnostic>& d, DiagCode code, NodeId node = 0) {
    for (const Diagnostic& x : d)
        if (x.code == code && (node == 0 || x.node == node)) return true;
    return false;
}

struct Chain {
    Graph g;
    NodeId box = 0, transform = 0, join = 0, boxB = 0, out = 0;
};

// Box -> Transform Geometry --\
//                              Join Geometry -> Output
// Box (second) --------------/
Chain makeChain() {
    Chain c;
    c.box = addNode(c.g, "Box").id;
    c.transform = addNode(c.g, "TransformGeometry").id;
    c.boxB = addNode(c.g, "Box").id;
    c.join = addNode(c.g, "JoinGeometry").id;
    c.out = addNode(c.g, "Output").id;
    setParam(*c.g.findNode(c.box), "Size", Vec3{2, 2, 2});
    setParam(*c.g.findNode(c.transform), "Translation", Vec3{10, 0, 0});
    addLink(c.g, c.box, "Geometry", c.transform, "Geometry");
    addLink(c.g, c.transform, "Geometry", c.join, "Geometry");
    addLink(c.g, c.boxB, "Geometry", c.join, "Geometry");
    addLink(c.g, c.join, "Geometry", c.out, "Geometry");
    return c;
}

}  // namespace

TEST(GraphEvaluator, BoxTransformJoinOutput) {
    Chain c = makeChain();
    EvalResult r = evaluateGraph(c.g, reg(), EvalContext{});
    ASSERT_TRUE(r.success) << (r.diagnostics.empty() ? "" : r.diagnostics[0].message);
    ASSERT_TRUE(r.geometry);
    EXPECT_EQ(r.geometry->vertexCount(), 48u);
    EXPECT_EQ(r.geometry->indices.size(), 72u);
    const Bounds b = computeBounds(*r.geometry);
    EXPECT_EQ(b.min, (Vec3{-0.5f, -1, -1}));  // second box is the default 1x1x1
    EXPECT_EQ(b.max, (Vec3{11, 1, 1}));
    EXPECT_EQ(r.outputNode, c.out);
    EXPECT_EQ(r.stats.nodesEvaluated, 5u);
}

TEST(GraphEvaluator, TypedSocketsAndExplicitConversion) {
    Chain c = makeChain();
    NodeId x = addNode(c.g, "FloatValue").id;
    NodeId combine = addNode(c.g, "CombineXYZ").id;
    setParam(*c.g.findNode(x), "Value", 7.0f);
    addLink(c.g, x, "Value", combine, "X");
    addLink(c.g, combine, "Vector", c.transform, "Translation");
    EvalResult r = evaluateGraph(c.g, reg(), EvalContext{});
    ASSERT_TRUE(r.success);
    EXPECT_EQ(computeBounds(*r.geometry).max.x, 8.0f);  // translated box spans 6..8

    // Float -> Vector3 directly is a type mismatch, never converted implicitly.
    Chain bad = makeChain();
    NodeId f = addNode(bad.g, "FloatValue").id;
    addLink(bad.g, f, "Value", bad.transform, "Translation");
    auto diags = validateGraph(bad.g, reg());
    EXPECT_TRUE(hasCode(diags, DiagCode::TypeMismatch, bad.transform));
    EXPECT_FALSE(evaluateGraph(bad.g, reg(), EvalContext{}).success);
}

TEST(GraphEvaluator, StructuralErrors) {
    {  // duplicate id
        Chain c = makeChain();
        c.g.nodes[1].id = c.g.nodes[0].id;
        EXPECT_TRUE(hasCode(validateGraph(c.g, reg()), DiagCode::DuplicateNodeId));
    }
    {  // link to a missing node
        Chain c = makeChain();
        addLink(c.g, 999, "Geometry", c.join, "Geometry");
        EXPECT_TRUE(hasCode(validateGraph(c.g, reg()), DiagCode::UnknownSocket));
    }
    {  // unknown socket
        Chain c = makeChain();
        c.g.links[0].from.socket = "Nope";
        EXPECT_TRUE(hasCode(validateGraph(c.g, reg()), DiagCode::UnknownSocket));
    }
    {  // two links into a single-link input
        Chain c = makeChain();
        addLink(c.g, c.boxB, "Geometry", c.transform, "Geometry");
        EXPECT_TRUE(hasCode(validateGraph(c.g, reg()), DiagCode::MultipleLinks, c.transform));
    }
    {  // required input unconnected
        Chain c = makeChain();
        c.g.links.erase(c.g.links.begin());  // Box -> Transform
        auto d = validateGraph(c.g, reg());
        EXPECT_TRUE(hasCode(d, DiagCode::MissingInput, c.transform));
    }
    {  // no / several outputs
        Chain c = makeChain();
        removeNode(c.g, c.out);
        EXPECT_TRUE(hasCode(validateGraph(c.g, reg()), DiagCode::NoOutputNode));
        Chain d = makeChain();
        addNode(d.g, "Output");
        EXPECT_TRUE(hasCode(validateGraph(d.g, reg()), DiagCode::MultipleOutputNodes));
    }
}

TEST(GraphEvaluator, CycleIsRejected) {
    Chain c = makeChain();
    // join -> transform -> join
    addLink(c.g, c.join, "Geometry", c.transform, "Geometry");
    c.g.links.erase(c.g.links.begin());  // drop Box -> Transform so the input has a single link
    auto d = validateGraph(c.g, reg());
    EXPECT_TRUE(hasCode(d, DiagCode::Cycle));
    EXPECT_FALSE(evaluateGraph(c.g, reg(), EvalContext{}).success);
}

TEST(GraphEvaluator, DisconnectedCycleIsRejected) {
    Chain c = makeChain();
    const NodeId a = addNode(c.g, "TransformGeometry").id;
    const NodeId b = addNode(c.g, "TransformGeometry").id;
    addLink(c.g, a, "Geometry", b, "Geometry");
    addLink(c.g, b, "Geometry", a, "Geometry");
    EXPECT_TRUE(hasCode(validateGraph(c.g, reg()), DiagCode::Cycle));
    EXPECT_FALSE(evaluateGraph(c.g, reg(), EvalContext{}).success);
}

TEST(GraphEvaluator, UnknownTypeIsDiagnosedOnlyWhenReachable) {
    Chain c = makeChain();
    Node& stray = addNode(c.g, "FutureNode", 3);
    stray.params["Foo"] = 1;
    EvalResult ok = evaluateGraph(c.g, reg(), EvalContext{});
    EXPECT_TRUE(ok.success);  // unreachable: kept, not evaluated, not an error
    EXPECT_NE(c.g.findNode(stray.id), nullptr);

    addLink(c.g, stray.id, "Geometry", c.join, "Geometry");
    auto d = validateGraph(c.g, reg());
    EXPECT_TRUE(hasCode(d, DiagCode::UnknownNodeType, stray.id));
}

TEST(GraphEvaluator, ParameterDiagnostics) {
    Chain c = makeChain();
    c.g.findNode(c.box)->params["Size"] = "big";            // wrong type
    c.g.findNode(c.boxB)->params["Colour"] = 1;             // unknown -> warning only
    auto d = validateGraph(c.g, reg());
    EXPECT_TRUE(hasCode(d, DiagCode::InvalidParameter, c.box));
    EXPECT_TRUE(hasCode(d, DiagCode::UnknownParameter, c.boxB));

    Chain w = makeChain();
    w.g.findNode(w.boxB)->params["Colour"] = 1;
    EXPECT_TRUE(evaluateGraph(w.g, reg(), EvalContext{}).success);  // warning does not block

    Chain v = makeChain();
    v.g.findNode(v.box)->version = 99;
    EXPECT_TRUE(hasCode(validateGraph(v.g, reg()), DiagCode::UnsupportedNodeVersion, v.box));
}

TEST(GraphEvaluator, EvaluationFailureReportsNodeAndSkipsDownstream) {
    Chain c = makeChain();
    setParam(*c.g.findNode(c.transform), "Scale", Vec3{1, 0, 1});  // singular
    EvalResult r = evaluateGraph(c.g, reg(), EvalContext{});
    EXPECT_FALSE(r.success);
    EXPECT_FALSE(r.geometry);
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::SingularTransform, c.transform));
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::Upstream, c.join));
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::Upstream, c.out));
}

TEST(GraphEvaluator, ResourceLimitsAreEnforcedBeforeAllocation) {
    Graph g;
    NodeId grid = addNode(g, "Grid").id;
    NodeId out = addNode(g, "Output").id;
    setParam(*g.findNode(grid), "VerticesX", int32_t(2000000000));
    setParam(*g.findNode(grid), "VerticesZ", int32_t(2000000000));
    addLink(g, grid, "Geometry", out, "Geometry");
    EvalResult r = evaluateGraph(g, reg(), EvalContext{});
    EXPECT_FALSE(r.success);
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::LimitExceeded, grid));

    // Joined size can exceed the limit even when every input fits.
    Chain c = makeChain();
    EvalContext ctx;
    ctx.limits.maxVertices = 40;  // 24 + 24 = 48
    EvalResult j = evaluateGraph(c.g, reg(), ctx);
    EXPECT_FALSE(j.success);
    EXPECT_TRUE(hasCode(j.diagnostics, DiagCode::LimitExceeded, c.join));
}

TEST(GraphEvaluator, CancellationStopsEvaluation) {
    Chain c = makeChain();
    std::atomic<bool> cancel{true};
    EvalContext ctx;
    ctx.cancel = &cancel;
    EvalResult r = evaluateGraph(c.g, reg(), ctx);
    EXPECT_FALSE(r.success);
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::Cancelled));
}

TEST(GraphEvaluator, CacheReusesUnchangedNodesAndInvalidatesDownstreamOnly) {
    Chain c = makeChain();
    EvalCache cache;
    EvalContext ctx;

    EvalResult first = evaluateGraph(c.g, reg(), ctx, &cache);
    ASSERT_TRUE(first.success);
    EXPECT_EQ(first.stats.nodesEvaluated, 5u);

    EvalResult again = evaluateGraph(c.g, reg(), ctx, &cache);
    ASSERT_TRUE(again.success);
    EXPECT_EQ(again.stats.nodesEvaluated, 0u);
    EXPECT_EQ(again.stats.nodesCached, 5u);
    EXPECT_EQ(again.geometry.get(), first.geometry.get());  // the very same immutable result

    // Editing the Transform recomputes Transform, Join, Output; both Boxes stay cached.
    setParam(*c.g.findNode(c.transform), "Translation", Vec3{20, 0, 0});
    EvalResult edited = evaluateGraph(c.g, reg(), ctx, &cache);
    ASSERT_TRUE(edited.success);
    EXPECT_EQ(edited.stats.nodesEvaluated, 3u);
    EXPECT_EQ(edited.stats.nodesCached, 2u);
    EXPECT_EQ(computeBounds(*edited.geometry).max.x, 21.0f);

    // Layout is not part of any key.
    c.g.layout[c.box] = NodeLayout{100, 200};
    EXPECT_EQ(evaluateGraph(c.g, reg(), ctx, &cache).stats.nodesEvaluated, 0u);

    // A different seed (context) invalidates everything.
    EvalContext seeded;
    seeded.seed = 7;
    EXPECT_EQ(evaluateGraph(c.g, reg(), seeded, &cache).stats.nodesEvaluated, 5u);

    // Removing a node prunes its cache entry.
    const size_t before = cache.size();
    removeNode(c.g, c.boxB);
    ASSERT_TRUE(evaluateGraph(c.g, reg(), seeded, &cache).success);
    EXPECT_EQ(cache.size(), before - 1);
}

TEST(GraphEvaluator, FailedNodeIsNotCachedAndRecovers) {
    Chain c = makeChain();
    EvalCache cache;
    Node& t = *c.g.findNode(c.transform);
    setParam(t, "Scale", Vec3{0, 1, 1});
    EXPECT_FALSE(evaluateGraph(c.g, reg(), EvalContext{}, &cache).success);
    setParam(t, "Scale", Vec3{1, 1, 1});
    EvalResult r = evaluateGraph(c.g, reg(), EvalContext{}, &cache);
    EXPECT_TRUE(r.success);
}

TEST(GraphEvaluator, DefinitionsExposeMetadataForUi) {
    const NodeDefinition* t = reg().find("TransformGeometry");
    ASSERT_NE(t, nullptr);
    const SocketDef* scale = t->findInput("Scale");
    ASSERT_NE(scale, nullptr);
    EXPECT_EQ(scale->type, SocketType::Vector3);
    EXPECT_EQ(std::get<Vec3>(scale->defaultValue), (Vec3{1, 1, 1}));
    EXPECT_TRUE(t->findInput("Geometry")->required);
    EXPECT_TRUE(reg().find("JoinGeometry")->findInput("Geometry")->multi);
    EXPECT_EQ(reg().find("Nope"), nullptr);
}
