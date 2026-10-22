#include "../Core/PlanarMeshCleanup.h"
#include "../Core/PlanarTriangulation.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>

using namespace Phantom::MeshEdit;
namespace {
void triangle(EditableMesh& m, uint32_t a, uint32_t b, uint32_t c) {
    std::vector<EditableCorner> loop;
    for (uint32_t id : {a, b, c}) {
        const auto* p = m.findVertex(id);
        EditableCorner corner;
        corner.vertex = id;
        corner.uv[0] = p->pos[0]; corner.uv[1] = p->pos[1];
        loop.push_back(corner);
    }
    m.addFace(loop);
}
EditableMesh fan() {
    EditableMesh m;
    m.addVertex(-1, -1, 0); m.addVertex(1, -1, 0);
    m.addVertex(1, 1, 0); m.addVertex(-1, 1, 0); m.addVertex(0, 0, 0);
    for (uint32_t i = 0; i < 4; ++i) triangle(m, i, (i + 1) % 4, 4);
    m.rebuildEdges(); m.revision = nextEditableRevision();
    return m;
}
double area(const EditableMesh& m) {
    double result = 0;
    for (const auto& f : m.faces) {
        const auto* a = m.findVertex(f.loop[0].vertex);
        const auto* b = m.findVertex(f.loop[1].vertex);
        const auto* c = m.findVertex(f.loop[2].vertex);
        using V = Phantom::Math::Vector3dd;
        result += glm::length(glm::cross(V(b->pos[0] - a->pos[0], b->pos[1] - a->pos[1], b->pos[2] - a->pos[2]),
                                        V(c->pos[0] - a->pos[0], c->pos[1] - a->pos[1], c->pos[2] - a->pos[2]))) / 2;
    }
    return result;
}
size_t boundaryEdges(const EditableMesh& m) {
    std::map<std::pair<uint32_t, uint32_t>, size_t> uses;
    for (const auto& f : m.faces)
        for (size_t k = 0; k < 3; ++k)
            ++uses[std::minmax(f.loop[k].vertex, f.loop[(k + 1) % 3].vertex)];
    return std::count_if(uses.begin(), uses.end(), [](const auto& e) { return e.second == 1; });
}
}
TEST(PlanarMeshCleanup, RemovesInteriorVertexAndPreservesAffineAttributes) {
    auto input = fan();
    for (auto& f : input.faces)
        for (auto& c : f.loop) { c.hasNormal = true; c.normal[2] = 1; }
    EditableMesh output;
    std::string error = "old";
    ASSERT_TRUE(cleanupPlanarMesh(input, output, {}, &error)) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_EQ(4u, output.vertices.size()); EXPECT_EQ(2u, output.faces.size());
    EXPECT_EQ(4u, input.faces.size()); EXPECT_NE(input.revision, output.revision);
    EXPECT_EQ(4u, boundaryEdges(output)); EXPECT_DOUBLE_EQ(4, area(output));
    for (const auto& f : output.faces) {
        EXPECT_GE(f.id, input.nextFaceId);
        for (const auto& c : f.loop) {
            const auto* p = output.findVertex(c.vertex);
            EXPECT_FLOAT_EQ(p->pos[0], c.uv[0]); EXPECT_FLOAT_EQ(p->pos[1], c.uv[1]);
            EXPECT_TRUE(c.hasNormal); EXPECT_FLOAT_EQ(1, c.normal[2]);
        }
    }
    EXPECT_EQ(nullptr, output.findVertex(4));
    ASSERT_TRUE(cleanupPlanarMesh(input, input, {}, &error)) << error;
    EXPECT_EQ(2u, input.faces.size());
}
TEST(PlanarMeshCleanup, RetainsAttributeGradientsSeamsAndNormalDiscontinuities) {
    for (int mode = 0; mode < 4; ++mode) {
        auto input = fan();
        for (size_t i = 0; i < input.faces.size(); ++i)
            for (auto& c : input.faces[i].loop) {
                if (mode == 0 && c.vertex == 4) c.uv[0] += .25f; // continuous UV, different gradients
                if (mode == 1) c.uv[0] += float(i); // actual UV seam
                if (mode == 2) { c.hasNormal = true; c.normal[2] = 1; if (c.vertex == 4) c.normal[0] = .2f; }
                if (mode == 3 && i == 0) { c.hasNormal = true; c.normal[2] = 1; }
            }
        EditableMesh output;
        ASSERT_TRUE(cleanupPlanarMesh(input, output));
        EXPECT_NE(nullptr, output.findVertex(4)) << mode;
        EXPECT_EQ(input.faces.size(), output.faces.size()) << mode;
    }
    auto input = fan();
    input.seams = {{0, 4}};
    EditableMesh output;
    ASSERT_TRUE(cleanupPlanarMesh(input, output));
    EXPECT_EQ(input.seams, output.seams); EXPECT_NE(nullptr, output.findEdge(0, 4));
    PlanarCleanupOptions options;
    options.protectedEdges = {{4, 0}};
    ASSERT_TRUE(cleanupPlanarMesh(fan(), output, options));
    EXPECT_NE(nullptr, output.findEdge(0, 4));
}
TEST(PlanarMeshCleanup, RemovesCollinearCreaseVertexAcrossBothPatches) {
    EditableMesh m;
    for (auto p : {std::array<float, 3>{-1, 0, 0}, {0, 0, 0}, {1, 0, 0},
                   {1, 1, 0}, {-1, 1, 0}, {-1, 0, 1}, {1, 0, 1}})
        m.addVertex(p[0], p[1], p[2]);
    triangle(m, 0, 1, 4); triangle(m, 1, 3, 4); triangle(m, 1, 2, 3);
    triangle(m, 2, 1, 6); triangle(m, 1, 5, 6); triangle(m, 1, 0, 5);
    m.rebuildEdges();
    EditableMesh out;
    ASSERT_TRUE(cleanupPlanarMesh(m, out));
    EXPECT_EQ(nullptr, out.findVertex(1)); EXPECT_EQ(4u, out.faces.size());
    EXPECT_NE(nullptr, out.findEdge(0, 2)); EXPECT_TRUE(out.validate());
    EXPECT_DOUBLE_EQ(area(m), area(out));
}
TEST(PlanarMeshCleanup, PreservesHoleAndDisconnectedComponents) {
    EditableMesh m;
    for (float scale : {2.f, 1.f})
        for (auto p : {std::array<float, 2>{-1, -1}, {1, -1}, {1, 1}, {-1, 1}})
            m.addVertex(p[0] * scale, p[1] * scale, 0);
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t j = (i + 1) % 4;
        const std::array<uint32_t, 4> ids{i, j, j + 4, i + 4};
        float x = 0, y = 0;
        for (auto id : ids) { x += m.findVertex(id)->pos[0]; y += m.findVertex(id)->pos[1]; }
        const auto center = m.addVertex(x / 4, y / 4, 0);
        for (size_t k = 0; k < 4; ++k) triangle(m, ids[k], ids[(k + 1) % 4], center);
    }
    const auto offset = m.nextVertexId;
    auto other = fan();
    for (const auto& v : other.vertices) m.addVertex(v.pos[0] + 8, v.pos[1], v.pos[2]);
    for (const auto& f : other.faces) triangle(m, f.loop[0].vertex + offset, f.loop[1].vertex + offset, f.loop[2].vertex + offset);
    m.rebuildEdges();
    EditableMesh out;
    ASSERT_TRUE(cleanupPlanarMesh(m, out));
    EXPECT_EQ(10u, out.faces.size()); EXPECT_EQ(12u, boundaryEdges(out));
    EXPECT_DOUBLE_EQ(16, area(out));
    // No triangle may cover the hole's center.
    for (const auto& f : out.faces) {
        bool allRight = true;
        for (const auto& c : f.loop) allRight = allRight && out.findVertex(c.vertex)->pos[0] > 4;
        if (allRight) continue;
        bool contains = true;
        for (size_t k = 0; k < 3; ++k) {
            const auto* a = out.findVertex(f.loop[k].vertex);
            const auto* b = out.findVertex(f.loop[(k + 1) % 3].vertex);
            contains = contains && double(b->pos[0] - a->pos[0]) * -a->pos[1] -
                                   double(b->pos[1] - a->pos[1]) * -a->pos[0] >= 0;
        }
        EXPECT_FALSE(contains);
    }
}
TEST(PlanarMeshCleanup, InvalidInputsBudgetsAndAliasingAreAtomic) {
    auto input = fan(), out = fan();
    const auto revision = out.revision;
    PlanarCleanupOptions options;
    std::string error;
    options.maxWork = 1;
    EXPECT_FALSE(cleanupPlanarMesh(input, out, options, &error));
    EXPECT_EQ(revision, out.revision); EXPECT_FALSE(error.empty());
    options = {}; options.relativeTolerance = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(cleanupPlanarMesh(input, out, options)); EXPECT_EQ(revision, out.revision);
    options = {}; options.protectedEdges = {{0, 99}};
    EXPECT_FALSE(cleanupPlanarMesh(input, out, options)); EXPECT_EQ(revision, out.revision);
    EXPECT_FALSE(cleanupPlanarMesh(makeEditableBox(), out)); EXPECT_EQ(revision, out.revision);
    input.nextFaceId = UINT32_MAX;
    EXPECT_FALSE(cleanupPlanarMesh(input, out)); EXPECT_EQ(revision, out.revision);
    input = fan(); input.faces[0].loop[0].vertex = 99;
    EXPECT_FALSE(cleanupPlanarMesh(input, out)); EXPECT_EQ(revision, out.revision);
}
TEST(PlanarMeshCleanup, BoundaryTriangulationKeepsCollinearVerticesWithoutCenters) {
    using V = Phantom::Math::Vector3dd;
    const std::vector<V> p{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}};
    std::vector<std::array<size_t, 3>> triangles;
    size_t work = 10000;
    ASSERT_TRUE(detail::triangulatePlanarBoundary(p, V(0, 0, 1), 1e-7, work, triangles));
    EXPECT_EQ(3u, triangles.size());
    std::set<size_t> used;
    for (auto t : triangles) for (auto i : t) used.insert(i);
    EXPECT_EQ(5u, used.size());
    const auto saved = triangles;
    work = 0;
    EXPECT_FALSE(detail::triangulatePlanarBoundary(p, V(0, 0, 1), 1e-7, work, triangles));
    EXPECT_EQ(saved, triangles);
}

TEST(PlanarMeshCleanup, DeterministicAndUnchangedMeshesKeepTheirRevision) {
    auto input = fan();
    EditableMesh a, b;
    ASSERT_TRUE(cleanupPlanarMesh(input, a));
    ASSERT_TRUE(cleanupPlanarMesh(input, b));
    ASSERT_EQ(a.faces.size(), b.faces.size());
    ASSERT_EQ(a.vertices.size(), b.vertices.size());
    for (size_t i = 0; i < a.faces.size(); ++i) {
        EXPECT_EQ(a.faces[i].id, b.faces[i].id);
        for (size_t k = 0; k < 3; ++k) EXPECT_EQ(a.faces[i].loop[k].vertex, b.faces[i].loop[k].vertex);
    }
    const auto revision = a.revision;
    ASSERT_TRUE(cleanupPlanarMesh(a, a));
    EXPECT_EQ(revision, a.revision);
    auto nonplanar = fan();
    nonplanar.vertices[4].pos[2] = .1f;
    ASSERT_TRUE(cleanupPlanarMesh(nonplanar, b));
    EXPECT_NE(nullptr, b.findVertex(4));
    EXPECT_EQ(nonplanar.revision, b.revision);
}

TEST(PlanarMeshCleanup, ImprovesUnconstrainedDiagonalAndKeepsProtectedDiagonal) {
    EditableMesh m;
    m.addVertex(0, 0, 0); m.addVertex(3, 0, 0);
    m.addVertex(2, 1, 0); m.addVertex(0, 2, 0);
    triangle(m, 0, 1, 3); triangle(m, 1, 2, 3);
    m.rebuildEdges();
    EditableMesh out;
    ASSERT_TRUE(cleanupPlanarMesh(m, out));
    EXPECT_NE(nullptr, out.findEdge(0, 2)); EXPECT_EQ(nullptr, out.findEdge(1, 3));
    EXPECT_DOUBLE_EQ(area(m), area(out));
    PlanarCleanupOptions options;
    options.protectedEdges = {{1, 3}};
    ASSERT_TRUE(cleanupPlanarMesh(m, out, options));
    EXPECT_NE(nullptr, out.findEdge(1, 3));
}
