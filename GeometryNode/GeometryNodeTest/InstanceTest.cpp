#include "gtest/gtest.h"

#include <cmath>

#include "../GeometryNode/GeometryOps.h"
#include "../GeometryNode/GraphEvaluator.h"
#include "../GeometryNode/GraphJson.h"

using namespace Phantom::GeometryNode;

namespace {

const NodeRegistry& reg() { return NodeRegistry::builtin(); }

NodeId add(Graph& g, const char* type) { return addNode(g, type).id; }

bool hasCode(const std::vector<Diagnostic>& d, DiagCode code) {
    for (const Diagnostic& x : d)
        if (x.code == code) return true;
    return false;
}

// Grid(4x4 vertices, 2x2 m) -> DistributePointsOnFaces -> InstanceOnPoints(Box 0.1) -> RealizeInstances -> Output
struct Scatter {
    Graph g;
    NodeId grid, dist, box, inst, realize, out;
};

Scatter makeScatter(int32_t count = 50, int32_t nodeSeed = 0) {
    Scatter s;
    s.grid = add(s.g, "Grid");
    s.dist = add(s.g, "DistributePointsOnFaces");
    s.box = add(s.g, "Box");
    s.inst = add(s.g, "InstanceOnPoints");
    s.realize = add(s.g, "RealizeInstances");
    s.out = add(s.g, "Output");
    setParam(*s.g.findNode(s.grid), "SizeX", 2.0f);
    setParam(*s.g.findNode(s.grid), "SizeZ", 2.0f);
    setParam(*s.g.findNode(s.grid), "VerticesX", int32_t(4));
    setParam(*s.g.findNode(s.grid), "VerticesZ", int32_t(4));
    setParam(*s.g.findNode(s.dist), "Count", count);
    setParam(*s.g.findNode(s.dist), "Seed", nodeSeed);
    setParam(*s.g.findNode(s.box), "Size", Vec3{0.1f, 0.1f, 0.1f});
    addLink(s.g, s.grid, "Geometry", s.dist, "Geometry");
    addLink(s.g, s.dist, "Points", s.inst, "Points");
    addLink(s.g, s.box, "Geometry", s.inst, "Instance");
    addLink(s.g, s.inst, "Instances", s.realize, "Geometry");
    addLink(s.g, s.realize, "Geometry", s.out, "Geometry");
    return s;
}

}  // namespace

TEST(Points, DistributeIsDeterministicAndOnTheSurface) {
    Scatter s = makeScatter(200);
    s.g.nodes.pop_back();  // Output; rebuild a points-only graph
    s.g.links.clear();
    const NodeId out = add(s.g, "Output");
    addLink(s.g, s.grid, "Geometry", s.dist, "Geometry");
    // Output refuses instances but accepts points (a mesh without triangles).
    addLink(s.g, s.dist, "Points", out, "Geometry");
    EvalContext ctx;
    ctx.seed = 7;
    const EvalResult a = evaluateGraph(s.g, reg(), ctx);
    const EvalResult b = evaluateGraph(s.g, reg(), ctx);
    ASSERT_TRUE(a.success) << (a.diagnostics.empty() ? "" : a.diagnostics[0].message);
    ASSERT_EQ(a.geometry->positions.size(), 200u);
    EXPECT_TRUE(a.geometry->indices.empty());
    EXPECT_EQ(a.geometry->positions, b.geometry->positions);
    for (const Vec3& p : a.geometry->positions) {
        EXPECT_NEAR(p.y, 0.0f, 1.0e-6f);
        EXPECT_GE(p.x, -1.0f - 1.0e-5f);
        EXPECT_LE(p.x, 1.0f + 1.0e-5f);
        EXPECT_GE(p.z, -1.0f - 1.0e-5f);
        EXPECT_LE(p.z, 1.0f + 1.0e-5f);
    }
    for (const Vec3& n : a.geometry->normals) EXPECT_NEAR(n.y, 1.0f, 1.0e-6f);

    // The context seed and the node Seed each change the result independently.
    ctx.seed = 8;
    EXPECT_NE(evaluateGraph(s.g, reg(), ctx).geometry->positions, a.geometry->positions);
    ctx.seed = 7;
    setParam(*s.g.findNode(s.dist), "Seed", int32_t(1));
    EXPECT_NE(evaluateGraph(s.g, reg(), ctx).geometry->positions, a.geometry->positions);
}

TEST(Points, DistributionFollowsTriangleArea) {
    // A 1x1 triangle pair where one triangle is 9x bigger: ~90% of points land in the big one.
    Mesh m;
    m.positions = {{0, 0, 0}, {3, 0, 0}, {0, 0, 3}, {10, 0, 10}, {10, 0, 11}, {11, 0, 10}};
    m.indices = {0, 2, 1, 3, 4, 5};
    Mesh out;
    ASSERT_EQ(distributePointsOnFaces(m, 4000, 1, 0, Limits{}, nullptr, out), OpStatus::Ok);
    size_t big = 0;
    for (const Vec3& p : out.positions) big += p.x < 5.0f;
    EXPECT_NEAR(double(big) / 4000.0, 4.5 / 5.0, 0.03);
}

TEST(Points, InstanceOnPointsKeepsReferencesUntilRealized) {
    Scatter s = makeScatter(10);
    // Without Realize the Output must refuse instances (never silently dropped / expanded).
    addLink(s.g, s.inst, "Instances", s.out, "Geometry");
    // Output has two links now; drop the realize path to make the graph valid.
    s.g.links.erase(s.g.links.begin() + 3, s.g.links.begin() + 5);
    const EvalResult bad = evaluateGraph(s.g, reg(), EvalContext{});
    EXPECT_FALSE(bad.success);
    EXPECT_TRUE(hasCode(bad.diagnostics, DiagCode::InvalidGeometry));
}

TEST(Points, RealizeExpandsEveryInstanceWithItsTransform) {
    Scatter s = makeScatter(10);
    const EvalResult r = evaluateGraph(s.g, reg(), EvalContext{});
    ASSERT_TRUE(r.success) << (r.diagnostics.empty() ? "" : r.diagnostics[0].message);
    EXPECT_EQ(r.geometry->positions.size(), 10u * 24u);
    EXPECT_EQ(r.geometry->indices.size(), 10u * 36u);
    EXPECT_TRUE(r.geometry->instances.empty());
    EXPECT_EQ(validateMesh(*r.geometry), OpStatus::Ok);
    const Bounds b = computeBounds(*r.geometry);
    EXPECT_GE(b.min.x, -1.06f);
    EXPECT_LE(b.max.x, 1.06f);
    EXPECT_NEAR(b.max.y, 0.05f, 1.0e-5f);
}

TEST(Points, PerPointScaleAndSelectionFields) {
    Scatter s = makeScatter(20);
    const NodeId vf = add(s.g, "VectorField");
    setParam(*s.g.findNode(vf), "Vector", Vec3{2, 2, 2});
    addLink(s.g, vf, "Vector", s.inst, "Scale");
    const EvalResult r = evaluateGraph(s.g, reg(), EvalContext{});
    ASSERT_TRUE(r.success);
    EXPECT_NEAR(computeBounds(*r.geometry).max.y, 0.1f, 1.0e-5f);

    // Selection = Index < 5 keeps only 5 instances.
    const NodeId idx = add(s.g, "Index");
    const NodeId lt = add(s.g, "MathLessThan");
    addLink(s.g, idx, "Index", lt, "A");
    setParam(*s.g.findNode(lt), "Value", 5.0f);
    addLink(s.g, lt, "Result", s.inst, "Selection");
    const EvalResult r2 = evaluateGraph(s.g, reg(), EvalContext{});
    ASSERT_TRUE(r2.success) << (r2.diagnostics.empty() ? "" : r2.diagnostics[0].message);
    EXPECT_EQ(r2.geometry->positions.size(), 5u * 24u);
}

TEST(Points, ZeroScaleIsRejected) {
    Scatter s = makeScatter(5);
    const NodeId vf = add(s.g, "VectorField");
    setParam(*s.g.findNode(vf), "Vector", Vec3{0, 1, 1});
    addLink(s.g, vf, "Vector", s.inst, "Scale");
    const EvalResult r = evaluateGraph(s.g, reg(), EvalContext{});
    EXPECT_FALSE(r.success);
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::SingularTransform));
}

TEST(Points, RealizeLimitIsEnforcedBeforeAllocating) {
    Scatter s = makeScatter(1000);
    EvalContext ctx;
    ctx.limits.maxVertices = 5000;  // 1000 * 24 would be 24000
    const EvalResult r = evaluateGraph(s.g, reg(), ctx);
    EXPECT_FALSE(r.success);
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::LimitExceeded));

    EvalContext few;
    few.limits.maxInstances = 100;
    EXPECT_TRUE(evaluateGraph(makeScatter(100).g, reg(), few).success);
    EXPECT_FALSE(evaluateGraph(makeScatter(101).g, reg(), few).success);
}

TEST(Points, OtherNodesRejectInstancesAndNestedInstancesAreRefused) {
    Scatter s = makeScatter(5);
    // Instances -> TransformGeometry -> Output.
    s.g.links.erase(s.g.links.begin() + 3, s.g.links.end());
    const NodeId tr = add(s.g, "TransformGeometry");
    addLink(s.g, s.inst, "Instances", tr, "Geometry");
    addLink(s.g, tr, "Geometry", s.out, "Geometry");
    const EvalResult r = evaluateGraph(s.g, reg(), EvalContext{});
    EXPECT_FALSE(r.success);
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::InvalidGeometry));

    // Nested: instance source that itself has instances.
    Scatter n = makeScatter(5);
    const NodeId inst2 = add(n.g, "InstanceOnPoints");
    n.g.links.erase(n.g.links.begin() + 3, n.g.links.end());
    addLink(n.g, n.dist, "Points", inst2, "Points");
    addLink(n.g, n.inst, "Instances", inst2, "Instance");
    addLink(n.g, inst2, "Instances", n.realize, "Geometry");
    addLink(n.g, n.realize, "Geometry", n.out, "Geometry");
    const EvalResult r2 = evaluateGraph(n.g, reg(), EvalContext{});
    EXPECT_FALSE(r2.success);
    EXPECT_TRUE(hasCode(r2.diagnostics, DiagCode::InvalidGeometry));
}

TEST(Points, MeshToPointsKeepsSelectedVertices) {
    Graph g;
    const NodeId box = add(g, "Box");
    const NodeId pts = add(g, "MeshToPoints");
    const NodeId out = add(g, "Output");
    addLink(g, box, "Geometry", pts, "Geometry");
    addLink(g, pts, "Points", out, "Geometry");
    const EvalResult r = evaluateGraph(g, reg(), EvalContext{});
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.geometry->positions.size(), 24u);
    EXPECT_TRUE(r.geometry->indices.empty());
    EXPECT_EQ(r.geometry->normals.size(), 24u);
}

TEST(Points, ScatterGraphSurvivesJsonRoundTrip) {
    Scatter s = makeScatter(30, 3);
    s.g.seed = 11;
    EvalContext ctx;
    ctx.seed = s.g.seed;
    const EvalResult a = evaluateGraph(s.g, reg(), ctx);
    Graph back;
    ASSERT_TRUE(parseGraph(serializeGraph(s.g), back).ok);
    const EvalResult b = evaluateGraph(back, reg(), ctx);
    ASSERT_TRUE(a.success && b.success);
    EXPECT_EQ(a.geometry->positions, b.geometry->positions);
}
