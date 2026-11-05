#include "gtest/gtest.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <limits>

#include "../Core/GeometryOps.h"
#include "../Core/GraphEvaluator.h"
#include "../Core/GraphJson.h"

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

TEST(Field, SharedBranchesEvaluateOncePerConsumer) {
    Mesh mesh;
    mesh.positions.resize(4);
    int calls = 0;
    FieldPtr f = makeFloatField([&](const FieldContext& ctx, std::vector<float>& out) {
        ++calls;
        out.assign(ctx.size(), static_cast<float>(ctx.seed + 1));
    });
    for (int depth = 0; depth < 24; ++depth) {
        f = makeFloatField([input = f](const FieldContext& ctx, std::vector<float>& out) {
            const auto a = evaluateFloats(input, ctx), b = evaluateFloats(input, ctx);
            if (ctx.stopped()) return;
            out.resize(a.size());
            for (size_t i = 0; i < a.size(); ++i) out[i] = a[i] + b[i];
        });
    }
    EXPECT_EQ(evaluateFloats(f, FieldContext{&mesh, 0}), std::vector<float>(4, 16777216.0f));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(evaluateFloats(f, FieldContext{&mesh, 1}), std::vector<float>(4, 33554432.0f));
    EXPECT_EQ(calls, 2); // a new consumer/seed must not reuse the previous arrays
}

TEST(Field, WorkspaceLimitAndCancellationStopBeforeAllocation) {
    Mesh mesh;
    mesh.positions.resize(4);
    int calls = 0;
    const FieldPtr f = makeFloatField([&](const FieldContext& ctx, std::vector<float>& out) {
        ++calls;
        out.assign(ctx.size(), 1.0f);
    });
    FieldEvaluation state;
    FieldContext ctx{&mesh, 0, Domain::Point, &state};
    ctx.maxMemoryBytes = 1;
    EXPECT_TRUE(evaluateFloats(f, ctx).empty());
    EXPECT_TRUE(state.limitExceeded);
    EXPECT_EQ(calls, 0);
    std::atomic<bool> cancel{true};
    FieldEvaluation cancelled;
    ctx.evaluation = &cancelled;
    ctx.cancel = &cancel;
    EXPECT_TRUE(evaluateFloats(f, ctx).empty());
    EXPECT_TRUE(cancelled.cancelled);
    EXPECT_EQ(calls, 0);
}

TEST(Field, WorkspaceFailureIsDiagnosedByConsumer) {
    Jitter j = makeJitter();
    EvalContext ctx;
    ctx.limits.maxMemoryBytes = 4096; // the Grid fits, but Set Position's workspace does not
    const auto r = evaluateGraph(j.g, reg(), ctx);
    EXPECT_FALSE(r.success);
    EXPECT_TRUE(hasCode(r.diagnostics, DiagCode::LimitExceeded, j.set));
}

TEST(Field, DeepExpressionsAreRejectedAndCancellationPropagates) {
    Mesh mesh;
    mesh.positions.resize(1);
    FieldPtr f = makeFloatField([](const FieldContext& ctx, std::vector<float>& out) { out.assign(ctx.size(), 1.0f); });
    for (int depth = 0; depth < 300; ++depth)
        f = makeFloatField([input = f](const FieldContext& ctx, std::vector<float>& out) { out = evaluateFloats(input, ctx); });
    FieldEvaluation state;
    FieldContext ctx{&mesh, 0, Domain::Point, &state};
    EXPECT_TRUE(evaluateFloats(f, ctx).empty());
    EXPECT_TRUE(state.limitExceeded);

    std::atomic<bool> cancel{false};
    f = makeFloatField([&](const FieldContext& c, std::vector<float>& out) {
        out.assign(c.size(), 1.0f);
        cancel.store(true);
    });
    f = makeFloatField([input = f](const FieldContext& c, std::vector<float>& out) { out = evaluateFloats(input, c); });
    FieldEvaluation cancelled;
    ctx.evaluation = &cancelled;
    ctx.cancel = &cancel;
    EXPECT_TRUE(evaluateFloats(f, ctx).empty());
    EXPECT_TRUE(cancelled.cancelled);
    EXPECT_TRUE(cancelled.results.empty());
}

TEST(Field, NoiseDisplacementAndRoundTrip) {
    for (const char* type : {"Noise2D", "Noise3D"}) {
        Jitter j=makeJitter(); j.g.findNode(j.random)->type=type;
        setParam(*j.g.findNode(j.random),"Scale",.73f);
        setParam(*j.g.findNode(j.random),"FractalMode",int32_t(1));
        const auto result=evaluateGraph(j.g,reg(),EvalContext{42});
        ASSERT_TRUE(result.success);
        Graph back; ASSERT_TRUE(parseGraph(serializeGraph(j.g),back).ok);
        const auto restored=evaluateGraph(back,reg(),EvalContext{42});
        ASSERT_TRUE(restored.success); EXPECT_EQ(result.geometry->positions,restored.geometry->positions);
        EXPECT_NE(result.geometry->positions,evaluateGraph(j.g,reg(),EvalContext{17}).geometry->positions);
        setParam(*j.g.findNode(j.random),"Scale",std::numeric_limits<float>::max());
        const auto invalid=evaluateGraph(j.g,reg(),EvalContext{});
        EXPECT_FALSE(invalid.success); EXPECT_TRUE(hasCode(invalid.diagnostics,DiagCode::InvalidParameter));
    }
}

TEST(Field, NoiseUsesCoordinatesAndOptionalPosition) {
    const auto* def=reg().find("Noise2D"); ASSERT_NE(def,nullptr);
    EvalContext eval; eval.seed=42;
    NodeEvalContext node(eval,1);
    for(const auto& input:def->inputs) node.inputs[input.id]={input.defaultValue};
    def->evaluate(node); ASSERT_FALSE(node.failed());
    auto field=std::get<FieldPtr>(node.outputs().at("Value"));
    Mesh mesh; mesh.positions={{.31f,0,.47f},{.31f,20,.47f},{.61f,0,.47f}};
    FieldContext ctx; ctx.mesh=&mesh;
    const auto values=evaluateFloats(field,ctx); ASSERT_EQ(values.size(),3u);
    EXPECT_FLOAT_EQ(values[0],values[1]); EXPECT_NE(values[0],values[2]);
    node.inputs["Position"]={Value(makeVectorField([](const FieldContext& c,std::vector<Vec3>& out){out.assign(c.size(),Vec3{.31f,100,.47f});}))};
    def->evaluate(node);
    field=std::get<FieldPtr>(node.outputs().at("Value"));
    const auto constant=evaluateFloats(field,ctx); ASSERT_EQ(constant.size(),3u);
    EXPECT_FLOAT_EQ(constant[0],values[0]); EXPECT_FLOAT_EQ(constant[0],constant[2]);
    node.inputs["Octaves"]={int32_t(0)}; def->evaluate(node); EXPECT_TRUE(node.failed());
}

TEST(Field, WarpedMultifractalRoundTripAndDiagnostics) {
    for (const char* type : {"DomainWarp2D", "DomainWarp3D"}) {
        Jitter j=makeJitter(); j.g.findNode(j.random)->type="Noise3D";
        const NodeId warp=add(j.g,type);
        addLink(j.g,warp,"Position",j.random,"Position");
        setParam(*j.g.findNode(j.random),"FractalMode",int32_t(4));
        setParam(*j.g.findNode(j.random),"Scale",.73f);
        setParam(*j.g.findNode(warp),"Scale",.43f);
        const auto warped=evaluateGraph(j.g,reg(),EvalContext{42}); ASSERT_TRUE(warped.success);
        Graph back; ASSERT_TRUE(parseGraph(serializeGraph(j.g),back).ok);
        const auto restored=evaluateGraph(back,reg(),EvalContext{42}); ASSERT_TRUE(restored.success);
        EXPECT_EQ(warped.geometry->positions,restored.geometry->positions);
        setParam(*j.g.findNode(warp),"Strength",0.0f);
        const auto plain=evaluateGraph(j.g,reg(),EvalContext{42}); ASSERT_TRUE(plain.success);
        EXPECT_NE(plain.geometry->positions,warped.geometry->positions);
        setParam(*j.g.findNode(warp),"Strength",-1.0f);
        const auto invalid=evaluateGraph(j.g,reg(),EvalContext{42});
        EXPECT_FALSE(invalid.success); EXPECT_TRUE(hasCode(invalid.diagnostics,DiagCode::InvalidParameter));
    }
    const auto* mode=reg().find("Noise2D")->findInput("FractalMode");
    ASSERT_NE(mode,nullptr); ASSERT_EQ(mode->choices.size(),5u);
    EXPECT_EQ(mode->choices.back().value,4);
}
TEST(Field, WarpDirectPositionAndCancellation) {
    Jitter j=makeJitter();
    const NodeId warp=add(j.g,"DomainWarp2D");
    // Replace the Offset connection with Warp as the absolute Position input.
    j.g.links.erase(std::remove_if(j.g.links.begin(),j.g.links.end(),[&](const auto& l){return l.to.node==j.set && l.to.socket=="Offset";}),j.g.links.end());
    addLink(j.g,warp,"Position",j.set,"Position");
    setParam(*j.g.findNode(warp),"Strength",0.0f);
    const auto result=evaluateGraph(j.g,reg(),EvalContext{}); ASSERT_TRUE(result.success);
    for(const auto& p:result.geometry->positions) EXPECT_FLOAT_EQ(p.y,0);
    std::atomic<bool> cancel{true}; EvalContext ctx; ctx.cancel=&cancel;
    EXPECT_FALSE(evaluateGraph(j.g,reg(),ctx).success);
}
