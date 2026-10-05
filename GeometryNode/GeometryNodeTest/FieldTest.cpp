#include "gtest/gtest.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "../GeometryNode/GeometryOps.h"
#include "../GeometryNode/GraphEvaluator.h"
#include "../GeometryNode/GraphJson.h"

using namespace Phantom::GeometryNode;

namespace {

const NodeRegistry& reg() { return NodeRegistry::builtin(); }

bool hasCode(const std::vector<Diagnostic>& d, DiagCode code, NodeId node = 0) {
    for (const Diagnostic& x : d)
        if (x.code == code && (node == 0 || x.node == node)) return true;
    return false;
}

NodeId add(Graph& g, const char* type) { return addNode(g, type).id; }

EvalResult evaluateGeometryOnly(const Graph& g) { return evaluateGraph(g, reg(), EvalContext{}); }

// Grid(5x5 = 25 points) -> SetPosition(Offset = Combine(Y = Random)) -> Output.
struct Jitter {
    Graph g;
    NodeId grid, random, combine, set, out;
};

Jitter makeJitter(int32_t nodeSeed = 0) {
    Jitter j;
    j.grid = add(j.g, "Grid");
    j.random = add(j.g, "RandomValue");
    j.combine = add(j.g, "CombineXYZField");
    j.set = add(j.g, "SetPosition");
    j.out = add(j.g, "Output");
    setParam(*j.g.findNode(j.grid), "VerticesX", int32_t(5));
    setParam(*j.g.findNode(j.grid), "VerticesZ", int32_t(5));
    setParam(*j.g.findNode(j.random), "Seed", nodeSeed);
    addLink(j.g, j.grid, "Geometry", j.set, "Geometry");
    addLink(j.g, j.random, "Value", j.combine, "Y");
    addLink(j.g, j.combine, "Vector", j.set, "Offset");
    addLink(j.g, j.set, "Geometry", j.out, "Geometry");
    return j;
}

std::vector<float> heights(const EvalResult& r) {
    std::vector<float> y;
    for (const Vec3& p : r.geometry->positions) y.push_back(p.y);
    return y;
}

}  // namespace

TEST(Field, RandomHashIsPinned) {
    // Golden values: the seed contract is frozen (changing it changes every saved random result).
    EXPECT_EQ(randomHash(0, 0, 0), 2802244911u);
    EXPECT_EQ(randomHash(0, 0, 1), 146079144u);
    EXPECT_EQ(randomHash(0, 0, 2), 3620509300u);
    EXPECT_EQ(randomHash(7, 0, 0), 3931604886u);
    EXPECT_EQ(randomHash(0, 5, 0), 4256354742u);
    EXPECT_EQ(randomHash(123, -1, 99), 233168647u);
    EXPECT_FLOAT_EQ(randomUnit(0, 0, 0), 0.65244848f);
    for (uint32_t i = 0; i < 1000; ++i) {
        const float u = randomUnit(3, 4, i);
        EXPECT_GE(u, 0.0f);
        EXPECT_LT(u, 1.0f);
    }
}

TEST(Field, SeedFixedDeformationIsReproducible) {
    Jitter j = makeJitter();
    j.g.seed = 42;
    const EvalResult a = evaluateGraph(j.g, reg(), EvalContext{42});
    // The graph seed is what the Studio passes as the context seed.
    EvalContext ctx;
    ctx.seed = j.g.seed;
    const EvalResult b = evaluateGraph(j.g, reg(), ctx);
    const EvalResult c = evaluateGraph(j.g, reg(), ctx);
    ASSERT_TRUE(b.success && c.success) << (b.diagnostics.empty() ? "" : b.diagnostics[0].message);
    EXPECT_EQ(b.geometry->positions, c.geometry->positions);
    EXPECT_EQ(a.geometry->positions, b.geometry->positions);
    const auto y = heights(b);
    ASSERT_EQ(y.size(), 25u);
    EXPECT_GT(*std::max_element(y.begin(), y.end()) - *std::min_element(y.begin(), y.end()), 0.2f);  // actually varies
    for (float v : y) {
        EXPECT_GE(v, 0.0f);
        EXPECT_LT(v, 1.0f);
    }

    // Round trip through JSON: same graph, same seed, same result.
    Graph back;
    ASSERT_TRUE(parseGraph(serializeGraph(j.g), back).ok);
    EXPECT_EQ(back.seed, 42u);
    ctx.seed = back.seed;
    const EvalResult d = evaluateGraph(back, reg(), ctx);
    ASSERT_TRUE(d.success);
    EXPECT_EQ(d.geometry->positions, b.geometry->positions);
}

TEST(Field, DifferentSeedsGiveDifferentResultsAndInvalidateTheCache) {
    Jitter j = makeJitter();
    EvalCache cache;
    EvalContext s0, s1;
    s1.seed = 1;
    const EvalResult a = evaluateGraph(j.g, reg(), s0, &cache);
    ASSERT_TRUE(a.success);
    EXPECT_EQ(evaluateGraph(j.g, reg(), s0, &cache).stats.nodesEvaluated, 0u);
    const EvalResult b = evaluateGraph(j.g, reg(), s1, &cache);
    ASSERT_TRUE(b.success);
    EXPECT_EQ(b.stats.nodesEvaluated, 5u);
    EXPECT_NE(a.geometry->positions, b.geometry->positions);

    // The node's own Seed is independent of the context seed.
    Jitter other = makeJitter(9);
    const EvalResult c = evaluateGraph(other.g, reg(), s0);
    ASSERT_TRUE(c.success);
    EXPECT_NE(a.geometry->positions, c.geometry->positions);
}

TEST(Field, SelectionByIndexMovesOnlySelectedPoints) {
    Graph g;
    const NodeId grid = add(g, "Grid"), index = add(g, "Index"), gt = add(g, "MathGreaterThan");
    const NodeId up = add(g, "VectorField"), set = add(g, "SetPosition"), out = add(g, "Output");
    setParam(*g.findNode(grid), "VerticesX", int32_t(5));
    setParam(*g.findNode(grid), "VerticesZ", int32_t(5));
    setParam(*g.findNode(gt), "Value", 11.5f);
    setParam(*g.findNode(up), "Vector", Vec3{0, 2, 0});
    addLink(g, index, "Index", gt, "A");
    addLink(g, grid, "Geometry", set, "Geometry");
    addLink(g, gt, "Result", set, "Selection");
    addLink(g, up, "Vector", set, "Offset");
    addLink(g, set, "Geometry", out, "Geometry");
    const EvalResult r = evaluateGraph(g, reg(), EvalContext{});
    ASSERT_TRUE(r.success);
    for (size_t i = 0; i < 25; ++i) EXPECT_EQ(r.geometry->positions[i].y, i > 11 ? 2.0f : 0.0f) << i;
    // Normals are recomputed and stay unit length.
    for (const Vec3& n : r.geometry->normals) EXPECT_NEAR(n.x * n.x + n.y * n.y + n.z * n.z, 1.0f, 1e-4f);
}

TEST(Field, PositionFieldAndVectorMath) {
    // new position = Position + (Position scaled by 2): everything doubles.
    Graph g;
    const NodeId box = add(g, "Box"), pos = add(g, "Position"), scale = add(g, "VectorScale"), set = add(g, "SetPosition"), out = add(g, "Output");
    setParam(*g.findNode(scale), "Value", 2.0f);
    addLink(g, pos, "Position", scale, "Vector");
    addLink(g, box, "Geometry", set, "Geometry");
    addLink(g, scale, "Result", set, "Position");
    addLink(g, set, "Geometry", out, "Geometry");
    const EvalResult r = evaluateGeometryOnly(g);
    ASSERT_TRUE(r.success);
    const Bounds b = computeBounds(*r.geometry);
    EXPECT_EQ(b.max, (Vec3{1, 1, 1}));
    EXPECT_EQ(b.min, (Vec3{-1, -1, -1}));
}

TEST(Field, DeleteGeometryRemovesPointsAndTheirTriangles) {
    Graph g;
    const NodeId box = add(g, "Box"), index = add(g, "Index"), lt = add(g, "MathLessThan"), del = add(g, "DeleteGeometry"), out = add(g, "Output");
    setParam(*g.findNode(lt), "Value", 12.0f);  // points 0..11 = the first three faces of the box
    addLink(g, index, "Index", lt, "A");
    addLink(g, box, "Geometry", del, "Geometry");
    addLink(g, lt, "Result", del, "Selection");
    addLink(g, del, "Geometry", out, "Geometry");
    const EvalResult r = evaluateGeometryOnly(g);
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.geometry->vertexCount(), 12u);
    EXPECT_EQ(r.geometry->triangleCount(), 6u);
    EXPECT_EQ(validateMesh(*r.geometry), OpStatus::Ok);
}

TEST(Field, DivideByZeroNeverProducesInfOrNaN) {
    Graph g;
    const NodeId grid = add(g, "Grid"), index = add(g, "Index"), div = add(g, "MathDivide"), comb = add(g, "CombineXYZField");
    const NodeId set = add(g, "SetPosition"), out = add(g, "Output");
    setParam(*g.findNode(div), "Value", 0.0f);
    addLink(g, index, "Index", div, "A");
    addLink(g, div, "Result", comb, "Y");
    addLink(g, grid, "Geometry", set, "Geometry");
    addLink(g, comb, "Vector", set, "Offset");
    addLink(g, set, "Geometry", out, "Geometry");
    const EvalResult r = evaluateGeometryOnly(g);
    ASSERT_TRUE(r.success);
    for (const Vec3& p : r.geometry->positions) EXPECT_EQ(p.y, 0.0f);
}

TEST(Field, FieldsAreADistinctTypeNeverConvertedImplicitly) {
    Graph g;
    const NodeId f = add(g, "FloatValue"), math = add(g, "MathAdd");
    const NodeId v = add(g, "CombineXYZ"), set = add(g, "SetPosition");
    addLink(g, f, "Value", math, "A");       // Float -> FieldFloat
    addLink(g, v, "Vector", set, "Offset");  // Vector3 -> FieldVector3
    const auto d = validateGraph(g, reg());
    EXPECT_TRUE(hasCode(d, DiagCode::TypeMismatch, math));
    EXPECT_TRUE(hasCode(d, DiagCode::TypeMismatch, set));
}

TEST(Field, MissingRequiredFieldIsDiagnosed) {
    Graph g;
    const NodeId box = add(g, "Box"), del = add(g, "DeleteGeometry"), out = add(g, "Output");
    addLink(g, box, "Geometry", del, "Geometry");
    addLink(g, del, "Geometry", out, "Geometry");
    EXPECT_TRUE(hasCode(validateGraph(g, reg()), DiagCode::MissingInput, del));  // Selection

}

TEST(Field, GraphSeedJson) {
    Graph g;
    ASSERT_TRUE(parseGraph(R"({"schema":1,"nodes":[],"links":[]})", g).ok);
    EXPECT_EQ(g.seed, 0u);  // files written before Phase 4
    ASSERT_TRUE(parseGraph(R"({"schema":1,"seed":4000000000,"nodes":[],"links":[]})", g).ok);
    EXPECT_EQ(g.seed, 4000000000u);
    EXPECT_NE(serializeGraph(g).find("\"seed\""), std::string::npos);
    EXPECT_FALSE(parseGraph(R"({"schema":1,"seed":-1,"nodes":[]})", g).ok);
    EXPECT_FALSE(parseGraph(R"({"schema":1,"seed":4294967296,"nodes":[]})", g).ok);
    EXPECT_EQ(g.seed, 4000000000u);  // untouched on failure
}
