#include "../Core/MeshReduction.h"
#include "../Core/EditableMeshConvert.h"
#include "../../Graphics/Core/PrimitiveMesh.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>

using namespace Phantom::MeshEdit;
namespace {
EditableMesh sphere(int stacks = 32, int slices = 64) {
    EditableMesh m;
    std::string error;
    EXPECT_TRUE(makeEditableFromMeshData(Phantom::Graphics::buildSphere(stacks, slices, 1), m, &error)) << error;
    return m;
}
size_t triangles(const EditableMesh& m) { size_t n = 0; for (const auto& f : m.faces) n += f.loop.size() - 2; return n; }
}
TEST(MeshReduction, SphereRatioErrorAndDeterminism) {
    const auto input = sphere();
    EditableMesh a, b;
    MeshReductionReport report, second;
    std::string error;
    ASSERT_TRUE(reduceMesh(input, {}, a, report, &error)) << error;
    ASSERT_TRUE(report.targetReached); EXPECT_TRUE(report.changed);
    EXPECT_EQ(triangles(input) / 2, report.outputTriangles);
    ASSERT_TRUE(a.validate(&error)) << error;
    for (const auto& f : a.faces) {
        ASSERT_EQ(3u, f.loop.size());
        std::array<double, 3> center{};
        for (size_t k = 0; k < 3; ++k) {
            const auto* p = a.findVertex(f.loop[k].vertex);
            const auto* q = a.findVertex(f.loop[(k + 1) % 3].vertex);
            double r = 0, mid = 0;
            for (int j = 0; j < 3; ++j) {
                center[j] += p->pos[j] / 3.; r += double(p->pos[j]) * p->pos[j];
                const double x = (double(p->pos[j]) + q->pos[j]) * .5; mid += x * x;
            }
            EXPECT_LT(std::abs(std::sqrt(r) - 1), .02 * std::sqrt(12.));
            EXPECT_LT(std::abs(std::sqrt(mid) - 1), .02 * std::sqrt(12.));
        }
        EXPECT_LT(std::abs(std::sqrt(center[0]*center[0]+center[1]*center[1]+center[2]*center[2]) - 1), .02 * std::sqrt(12.));
    }
    ASSERT_TRUE(reduceMesh(input, {}, b, second, &error)) << error;
    ASSERT_EQ(a.vertices.size(), b.vertices.size()); ASSERT_EQ(a.faces.size(), b.faces.size());
    for (size_t i = 0; i < a.vertices.size(); ++i) for (int k = 0; k < 3; ++k) EXPECT_EQ(a.vertices[i].pos[k], b.vertices[i].pos[k]);
    for (size_t i = 0; i < a.faces.size(); ++i) for (size_t k = 0; k < 3; ++k) EXPECT_EQ(a.faces[i].loop[k].vertex, b.faces[i].loop[k].vertex);
    std::printf("[Reduction] sphere %zu -> %zu, work=%zu\n", report.inputTriangles, report.outputTriangles, report.work);
}
TEST(MeshReduction, PlaneQuadsBordersAndSeams) {
    auto input = makeEditablePlane(1, 16);
    // An explicit interior seam must keep its edge and endpoints.
    input.seams = {{144, 145}};
    EditableMesh out;
    MeshReductionReport report;
    std::string error;
    ASSERT_TRUE(reduceMesh(input, {}, out, report, &error)) << error;
    EXPECT_TRUE(report.targetReached); EXPECT_EQ(256u, report.outputTriangles);
    EXPECT_NE(nullptr, out.findEdge(144, 145)); EXPECT_EQ(input.seams, out.seams);
    for (const auto& v : input.vertices) if (std::abs(v.pos[0]) == 1 || std::abs(v.pos[2]) == 1 || v.id == 144 || v.id == 145) {
        const auto* p = out.findVertex(v.id); ASSERT_NE(nullptr, p);
        for (int k = 0; k < 3; ++k) EXPECT_EQ(v.pos[k], p->pos[k]);
    }
}
TEST(MeshReduction, TorusHoleAndExplicitCount) {
    EditableMesh input, out;
    ASSERT_TRUE(makeEditableFromMeshData(Phantom::Graphics::buildTorus(1, .25f, 48, 24), input));
    MeshReductionOptions options; options.targetTriangles = 1201;
    MeshReductionReport report;
    std::string error;
    ASSERT_TRUE(reduceMesh(input, options, out, report, &error)) << error;
    EXPECT_TRUE(report.targetReached); EXPECT_EQ(1200u, out.faces.size());
    EXPECT_EQ(1201u, report.targetTriangles); // a two-face collapse can undershoot an odd target by one
    EXPECT_EQ(0, static_cast<int>(out.vertices.size()) - static_cast<int>(out.edges.size()) + static_cast<int>(out.faces.size()));
    for (const auto& v : out.vertices) EXPECT_GT(std::hypot(v.pos[0], v.pos[2]), .70);
}
TEST(MeshReduction, ProtectedTargetNoOpAndAtomicFailures) {
    auto box = makeEditableBox(), out = sphere(8,16);
    MeshReductionReport report;
    std::string error;
    ASSERT_TRUE(reduceMesh(box, {}, out, report, &error)) << error;
    EXPECT_FALSE(report.changed); EXPECT_FALSE(report.targetReached);
    EXPECT_EQ(box.revision, out.revision); EXPECT_EQ(6u, out.faces.size());
    const auto revision = out.revision;
    report.work = 123;
    MeshReductionOptions options; options.maxWork = 2;
    EXPECT_FALSE(reduceMesh(box, options, out, report, &error)); EXPECT_EQ(123u, report.work); EXPECT_EQ(revision, out.revision);
    options = {}; options.ratio = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(reduceMesh(box, options, out, report)); EXPECT_EQ(revision, out.revision);
    options = {}; options.maxInputTriangles = 1;
    EXPECT_FALSE(reduceMesh(box, options, out, report));
    auto input = sphere(8,16); options = {}; options.ratio = 1;
    ASSERT_TRUE(reduceMesh(input, options, input, report)); EXPECT_FALSE(report.changed);
    input.nextFaceId = UINT32_MAX; options = {};
    EXPECT_FALSE(reduceMesh(input, options, out, report)); EXPECT_EQ(revision, out.revision);
}
TEST(MeshReduction, RejectsSelfIntersectionAndDisconnectedVertexFan) {
    auto m = makeEditablePlane(1,2);
    // Fold one corner across a nonadjacent triangle without changing topology.
    m.vertices[0].pos[0] = .8f; m.vertices[0].pos[2] = .8f;
    EditableMesh out;
    MeshReductionReport report;
    EXPECT_FALSE(reduceMesh(m, {}, out, report));
}

TEST(MeshReduction, AttributeDiscontinuitiesAreFixedAndComponentsStaySeparate) {
    for (bool normals : {false, true}) {
        auto input = sphere(8,16);
        for (size_t i = 0; i < input.faces.size(); ++i) for (auto& c : input.faces[i].loop) {
            if (normals) { c.hasNormal = true; c.normal[0] = float(i % 2); c.normal[1] = 0; c.normal[2] = float(1 - i % 2); }
            else c.uv[0] += float(i);
        }
        EditableMesh out;
        MeshReductionReport report;
        ASSERT_TRUE(reduceMesh(input, {}, out, report));
        EXPECT_FALSE(report.changed); EXPECT_EQ(input.revision, out.revision);
    }
    auto input = makeEditablePlane(1,8), second = makeEditablePlane(1,8);
    const auto offset = input.nextVertexId;
    for (const auto& v : second.vertices) input.addVertex(v.pos[0] + 5, v.pos[1], v.pos[2]);
    for (const auto& f : second.faces) {
        auto loop = f.loop;
        for (auto& c : loop) c.vertex += offset;
        input.addFace(loop);
    }
    input.rebuildEdges();
    EditableMesh out;
    MeshReductionReport report;
    ASSERT_TRUE(reduceMesh(input, {}, out, report));
    EXPECT_TRUE(report.targetReached);
    EXPECT_EQ(2, int(out.vertices.size()) - int(out.edges.size()) + int(out.faces.size()));
    for (const auto& v : out.vertices) EXPECT_TRUE(v.pos[0] <= 1 || v.pos[0] >= 4);
    MeshReductionOptions options; options.maxWork = 10000;
    const auto revision = out.revision;
    EXPECT_FALSE(reduceMesh(sphere(), options, out, report));
    EXPECT_EQ(revision, out.revision);
}
