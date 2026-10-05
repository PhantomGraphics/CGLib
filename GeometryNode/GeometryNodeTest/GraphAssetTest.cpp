#include "gtest/gtest.h"

#include "../GeometryNode/GeometryOps.h"
#include "../GeometryNode/GraphAsset.h"
#include "../GeometryNode/GraphEvaluator.h"
#include "../GeometryNode/GraphJson.h"

using namespace Phantom::GeometryNode;

namespace {

const NodeRegistry& reg() { return NodeRegistry::builtin(); }

// Box -> Transform -> Output; Transform.Translation and Box.Size are exposed.
struct Asset {
    Graph g;
    NodeId box, tr, out;
};

Asset makeAsset() {
    Asset a;
    a.box = addNode(a.g, "Box").id;
    a.tr = addNode(a.g, "TransformGeometry").id;
    a.out = addNode(a.g, "Output").id;
    addLink(a.g, a.box, "Geometry", a.tr, "Geometry");
    addLink(a.g, a.tr, "Geometry", a.out, "Geometry");
    return a;
}

}  // namespace

TEST(GraphAsset, ExposeValidatesTheSocket) {
    Asset a = makeAsset();
    std::string err;
    EXPECT_TRUE(addExposed(a.g, "size", a.box, "Size", reg(), &err)) << err;
    EXPECT_TRUE(addExposed(a.g, "offset", a.tr, "Translation", reg(), &err)) << err;
    EXPECT_FALSE(addExposed(a.g, "size", a.tr, "Scale", reg(), &err));          // duplicate name
    EXPECT_FALSE(addExposed(a.g, "bad name", a.tr, "Scale", reg(), &err));      // illegal character
    EXPECT_FALSE(addExposed(a.g, "geo", a.tr, "Geometry", reg(), &err));        // geometry sockets cannot be exposed
    EXPECT_FALSE(addExposed(a.g, "ghost", 99, "Size", reg(), &err));            // unknown node
    EXPECT_FALSE(addExposed(a.g, "nope", a.box, "NoSuchSocket", reg(), &err));  // unknown socket
    EXPECT_EQ(a.g.exposed.size(), 2u);

    const auto info = describeExposed(a.g, reg());
    ASSERT_EQ(info.size(), 2u);
    EXPECT_TRUE(info[0].valid);
    EXPECT_EQ(info[0].type, SocketType::Vector3);
    EXPECT_EQ(info[0].value, nlohmann::json::array({1.0, 1.0, 1.0}));  // the socket default

    // A linked socket cannot be exposed, and linking an exposed one makes it report invalid (never silently dropped).
    const NodeId f = addNode(a.g, "FloatValue").id;
    const NodeId c = addNode(a.g, "CombineXYZ").id;
    addLink(a.g, f, "Value", c, "X");
    addLink(a.g, c, "Vector", a.tr, "Translation");
    EXPECT_FALSE(describeExposed(a.g, reg())[1].valid);
    EXPECT_FALSE(addExposed(a.g, "x", c, "X", reg(), &err));
}

TEST(GraphAsset, OverridesProduceTheEffectiveGraph) {
    Asset a = makeAsset();
    ASSERT_TRUE(addExposed(a.g, "offset", a.tr, "Translation", reg()));
    const EvalResult base = evaluateGraph(a.g, reg(), EvalContext{});
    ASSERT_TRUE(base.success);

    Graph one, two;
    std::string err;
    ASSERT_TRUE(applyOverrides(a.g, {{"offset", nlohmann::json::array({10.0, 0.0, 0.0})}}, reg(), one, &err)) << err;
    ASSERT_TRUE(applyOverrides(a.g, {{"offset", nlohmann::json::array({0.0, 5.0, 0.0})}}, reg(), two, &err)) << err;
    const EvalResult r1 = evaluateGraph(one, reg(), EvalContext{});
    const EvalResult r2 = evaluateGraph(two, reg(), EvalContext{});
    ASSERT_TRUE(r1.success && r2.success);
    EXPECT_NEAR(computeBounds(*r1.geometry).max.x, 10.5f, 1e-5f);
    EXPECT_NEAR(computeBounds(*r2.geometry).max.y, 5.5f, 1e-5f);
    EXPECT_NEAR(computeBounds(*base.geometry).max.x, 0.5f, 1e-5f);  // the asset itself is untouched
    EXPECT_TRUE(a.g.findNode(a.tr)->params.empty());
}

TEST(GraphAsset, BadOverridesAreRejectedAndChangeNothing) {
    Asset a = makeAsset();
    ASSERT_TRUE(addExposed(a.g, "offset", a.tr, "Translation", reg()));
    Graph out;
    out.seed = 77;
    std::string err;
    EXPECT_FALSE(applyOverrides(a.g, {{"unknown", 1.0}}, reg(), out, &err));
    EXPECT_FALSE(err.empty());
    EXPECT_FALSE(applyOverrides(a.g, {{"offset", 3.0}}, reg(), out, &err));  // a float is not a vector
    EXPECT_FALSE(applyOverrides(a.g, {{"offset", "text"}}, reg(), out, &err));
    EXPECT_EQ(out.seed, 77u);  // untouched on failure
    EXPECT_TRUE(applyOverrides(a.g, {}, reg(), out, &err));  // no overrides = the asset itself
    EXPECT_EQ(out.nodes.size(), 3u);
}

TEST(GraphAsset, ExposedParametersRoundTripThroughJson) {
    Asset a = makeAsset();
    ASSERT_TRUE(addExposed(a.g, "offset", a.tr, "Translation", reg()));
    ASSERT_TRUE(addExposed(a.g, "size", a.box, "Size", reg()));
    Graph back;
    ASSERT_TRUE(parseGraph(serializeGraph(a.g), back).ok);
    ASSERT_EQ(back.exposed.size(), 2u);
    EXPECT_EQ(back.exposed[0].name, "offset");
    EXPECT_EQ(back.exposed[0].node, a.tr);
    EXPECT_EQ(back.exposed[1].socket, "Size");
    EXPECT_TRUE(back.extra.empty());  // "exposed" is a known key, not preserved as an unknown one

    nlohmann::json bad = graphToJson(a.g);
    bad["exposed"][0].erase("node");
    EXPECT_FALSE(graphFromJson(bad, back).ok);
    EXPECT_TRUE(removeExposed(a.g, "size"));
    EXPECT_FALSE(removeExposed(a.g, "size"));
    EXPECT_EQ(a.g.exposed.size(), 1u);
}

TEST(GraphAsset, DifferentOverridesDoNotShareCacheEntriesWrongly) {
    Asset a = makeAsset();
    ASSERT_TRUE(addExposed(a.g, "offset", a.tr, "Translation", reg()));
    EvalCache cache;
    Graph one, two;
    ASSERT_TRUE(applyOverrides(a.g, {{"offset", nlohmann::json::array({1.0, 0.0, 0.0})}}, reg(), one));
    ASSERT_TRUE(applyOverrides(a.g, {{"offset", nlohmann::json::array({2.0, 0.0, 0.0})}}, reg(), two));
    const EvalResult r1 = evaluateGraph(one, reg(), EvalContext{}, &cache);
    const EvalResult r2 = evaluateGraph(two, reg(), EvalContext{}, &cache);
    ASSERT_TRUE(r1.success && r2.success);
    EXPECT_NEAR(computeBounds(*r1.geometry).max.x, 1.5f, 1e-5f);
    EXPECT_NEAR(computeBounds(*r2.geometry).max.x, 2.5f, 1e-5f);
    EXPECT_EQ(r2.stats.nodesCached, 1u);  // the Box node is reused; only Transform/Output are recomputed
}
