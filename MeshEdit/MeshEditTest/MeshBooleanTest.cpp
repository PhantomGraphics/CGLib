#include "../Core/MeshBoolean.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <map>
#include <set>
using namespace Phantom::MeshEdit;
namespace {
EditableMesh box(float x = 0, float y = 0, float z = 0, float h = 0.5f) {
    auto m = makeEditableBox(h);
    for (auto& v : m.vertices) {
        v.pos[0] += x;
        v.pos[1] += y;
        v.pos[2] += z;
    }
    return m;
}
double volume(const EditableMesh& m) {
    double v = 0;
    for (const auto& f : m.faces) {
        auto a = m.findVertex(f.loop[0].vertex);
        for (size_t i = 1; i + 1 < f.loop.size(); ++i) {
            auto b = m.findVertex(f.loop[i].vertex), c = m.findVertex(f.loop[i + 1].vertex);
            v += (double(a->pos[0]) * (double(b->pos[1]) * c->pos[2] - double(b->pos[2]) * c->pos[1]) +
                  double(a->pos[1]) * (double(b->pos[2]) * c->pos[0] - double(b->pos[0]) * c->pos[2]) +
                  double(a->pos[2]) * (double(b->pos[0]) * c->pos[1] - double(b->pos[1]) * c->pos[0])) /
                 6;
        }
    }
    return v;
}
void check(const EditableMesh& a, const EditableMesh& b, BooleanOperation op, double expected) {
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(booleanMeshes(a, b, op, out, {}, &error)) << error;
    EXPECT_TRUE(out.validate(&error)) << error;
    EXPECT_NEAR(expected, volume(out), 1e-5);
    for (const auto& v : out.vertices)
        EXPECT_GE(v.id, a.nextVertexId);
    for (const auto& f : out.faces) {
        EXPECT_GE(f.id, a.nextFaceId);
        EXPECT_EQ(3u, f.loop.size());
    }
    EXPECT_GT(out.revision, 0u);
}
EditableMesh combine(EditableMesh a, const EditableMesh& b, bool reverse = false) {
    std::map<uint32_t, uint32_t> ids;
    for (auto v : b.vertices)
        ids[v.id] = a.addVertex(v.pos[0], v.pos[1], v.pos[2]);
    for (auto f : b.faces) {
        for (auto& c : f.loop)
            c.vertex = ids[c.vertex];
        if (reverse)
            std::reverse(f.loop.begin(), f.loop.end());
        a.addFace(f.loop);
    }
    a.rebuildEdges();
    return a;
}
} // namespace
TEST(MeshBoolean, OverlappingBoxes) {
    auto a = box(), b = box(.5f);
    check(a, b, BooleanOperation::Union, 1.5);
    check(a, b, BooleanOperation::Difference, .5);
    check(a, b, BooleanOperation::Intersection, .5);
}
TEST(MeshBoolean, DisjointContainedAndIdentical) {
    auto a = box();
    check(a, box(2), BooleanOperation::Union, 2);
    check(a, box(2), BooleanOperation::Difference, 1);
    check(a, box(2), BooleanOperation::Intersection, 0);
    check(a, a, BooleanOperation::Union, 1);
    check(a, a, BooleanOperation::Difference, 0);
    check(a, a, BooleanOperation::Intersection, 1);
    check(a, box(0, 0, 0, .25f), BooleanOperation::Difference, .875);
    check(a, box(0, 0, 0, .25f), BooleanOperation::Intersection, .125);
}
TEST(MeshBoolean, FaceContact) {
    check(box(), box(1), BooleanOperation::Union, 2);
    check(box(), box(1), BooleanOperation::Difference, 1);
    check(box(), box(1), BooleanOperation::Intersection, 0);
}
TEST(MeshBoolean, EdgeAndPointContactRejectNonManifoldUnion) {
    for (auto b : {box(1, 1), box(1, 1, 1)}) {
        EditableMesh out;
        std::string e;
        EXPECT_FALSE(booleanMeshes(box(), b, BooleanOperation::Union, out, {}, &e));
        EXPECT_FALSE(e.empty());
        check(box(), b, BooleanOperation::Intersection, 0);
        check(box(), b, BooleanOperation::Difference, 1);
    }
}
TEST(MeshBoolean, EmptySetAndAliasing) {
    EditableMesh empty;
    check(empty, box(), BooleanOperation::Union, 1);
    check(box(), empty, BooleanOperation::Difference, 1);
    check(empty, box(), BooleanOperation::Difference, 0);
    check(box(), empty, BooleanOperation::Intersection, 0);
    auto a = box();
    std::string e;
    ASSERT_TRUE(booleanMeshes(a, box(.5f), BooleanOperation::Difference, a, {}, &e)) << e;
    EXPECT_NEAR(.5, volume(a), 1e-5);
}
TEST(MeshBoolean, ConcaveDisconnectedAndCavityInputs) {
    EditableMesh concave;
    std::string e;
    ASSERT_TRUE(booleanMeshes(box(), box(.5f, .5f), BooleanOperation::Union, concave, {}, &e)) << e;
    check(concave, box(3), BooleanOperation::Union, 2.75);
    auto disconnected = combine(box(), box(3));
    check(disconnected, box(6), BooleanOperation::Union, 3);
    auto cavity = combine(box(0, 0, 0, 1), box(), true);
    check(cavity, box(4), BooleanOperation::Union, 8);
    check(cavity, box(0, 0, 0, .25f), BooleanOperation::Intersection, 0);
}
TEST(MeshBoolean, RotatedAndScaledCases) {
    auto a = box(), b = box(.35f, .2f);
    for (auto& v : b.vertices) {
        float x = v.pos[0], z = v.pos[2];
        v.pos[0] = .8f * x - .6f * z;
        v.pos[2] = .6f * x + .8f * z;
    }
    EditableMesh u, i;
    std::string e;
    ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Union, u, {}, &e)) << e;
    ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Intersection, i, {}, &e)) << e;
    EXPECT_NEAR(2, volume(u) + volume(i), 1e-5);
    for (float scale : {.01f, 100.f}) {
        auto x = box(), y = box(.5f);
        for (auto* m : {&x, &y})
            for (auto& v : m->vertices)
                for (float& p : v.pos)
                    p = p * scale + scale * 10;
        check(x, y, BooleanOperation::Intersection, .5 * scale * scale * scale);
    }
}
TEST(MeshBoolean, RejectsInvalidInputsAndPreservesOutput) {
    auto a = box(), out = box(9);
    auto revision = out.revision;
    std::string e;
    auto bad = box();
    bad.faces.pop_back();
    bad.rebuildEdges();
    EXPECT_FALSE(booleanMeshes(a, bad, BooleanOperation::Union, out, {}, &e));
    EXPECT_EQ(revision, out.revision);
    bad = box();
    for (auto& f : bad.faces)
        std::reverse(f.loop.begin(), f.loop.end());
    bad.rebuildEdges();
    EXPECT_FALSE(booleanMeshes(a, bad, BooleanOperation::Union, out, {}, &e));
    bad = combine(box(), box(.25f));
    EXPECT_FALSE(booleanMeshes(a, bad, BooleanOperation::Union, out, {}, &e));
    bad = box();
    bad.vertices[0].pos[0] = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(booleanMeshes(a, bad, BooleanOperation::Union, out, {}, &e));
    bad = box();
    bad.vertices[0].pos[0] += .1f;
    EXPECT_FALSE(booleanMeshes(a, bad, BooleanOperation::Union, out, {}, &e));
    EXPECT_EQ(revision, out.revision);
}
TEST(MeshBoolean, LimitsAndOptionsAreAtomic) {
    auto a = box(), out = box(9);
    auto rev = out.revision;
    std::string e;
    BooleanOptions o;
    o.maxInputTriangles = 1;
    EXPECT_FALSE(booleanMeshes(a, box(.5f), BooleanOperation::Union, out, o, &e));
    o = {};
    o.maxIntermediatePolygons = 1;
    EXPECT_FALSE(booleanMeshes(a, box(.5f), BooleanOperation::Union, out, o, &e));
    o = {};
    o.maxOutputFaces = 1;
    EXPECT_FALSE(booleanMeshes(a, box(.5f), BooleanOperation::Union, out, o, &e));
    o = {};
    o.relativeTolerance = 0;
    EXPECT_FALSE(booleanMeshes(a, box(.5f), BooleanOperation::Union, out, o, &e));
    EXPECT_EQ(rev, out.revision);
}
TEST(MeshBoolean, AttributesAndDeterminism) {
    auto a = box(), b = box(.5f);
    for (auto* m : {&a, &b})
        for (auto& f : m->faces)
            for (auto& c : f.loop) {
                c.uv[0] = 2;
                c.uv[1] = 3;
                c.hasNormal = true;
                c.normal[0] = 1;
                c.normal[1] = c.normal[2] = 0;
            }
    EditableMesh x, y;
    std::string e;
    ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Difference, x, {}, &e)) << e;
    ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Difference, y, {}, &e)) << e;
    ASSERT_EQ(x.vertices.size(), y.vertices.size());
    for (size_t i = 0; i < x.vertices.size(); ++i)
        for (int k = 0; k < 3; ++k)
            EXPECT_EQ(x.vertices[i].pos[k], y.vertices[i].pos[k]);
    bool flipped = false;
    for (const auto& f : x.faces)
        for (const auto& c : f.loop) {
            EXPECT_FLOAT_EQ(2, c.uv[0]);
            EXPECT_FLOAT_EQ(3, c.uv[1]);
            EXPECT_TRUE(c.hasNormal);
            flipped = flipped || c.normal[0] < 0;
        }
    EXPECT_TRUE(flipped);
    EXPECT_NE(x.revision, y.revision);
}

TEST(MeshBoolean, ConcaveQuadAndCoplanarPartialOverlap) {
    EditableMesh prism;
    for (float z : {-0.5f, 0.5f})
        for (auto p : {std::array<float, 2>{0, 0}, {2, 0}, {.5f, .5f}, {0, 2}})
            prism.addVertex(p[0], p[1], z);
    auto add = [&](std::initializer_list<uint32_t> ids) {
        std::vector<EditableCorner> corners;
        for (uint32_t id : ids) {
            EditableCorner c;
            c.vertex = id;
            corners.push_back(c);
        }
        prism.addFace(corners);
    };
    add({3, 2, 1, 0});
    add({4, 5, 6, 7});
    add({0, 1, 5, 4});
    add({1, 2, 6, 5});
    add({2, 3, 7, 6});
    add({3, 0, 4, 7});
    prism.rebuildEdges();
    check(prism, box(5), BooleanOperation::Union, 2);
    check(box(), box(1, .25f), BooleanOperation::Union, 2);
    check(box(), box(1, .25f), BooleanOperation::Difference, 1);
    check(box(), box(.5f, .25f), BooleanOperation::Intersection, .375);
}

TEST(MeshBoolean, InterpolatedUvAndSeamsAreRetained) {
    auto a = box(), b = box(.35f, .2f);
    for (auto* m : {&a, &b})
        for (auto& f : m->faces)
            for (auto& c : f.loop) {
                const auto* p = m->findVertex(c.vertex);
                c.uv[0] = p->pos[0];
                c.uv[1] = p->pos[1];
            }
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Difference, out, {}, &error)) << error;
    for (const auto& f : out.faces)
        for (const auto& c : f.loop) {
            const auto* p = out.findVertex(c.vertex);
            EXPECT_NEAR(p->pos[0], c.uv[0], 1e-6);
            EXPECT_NEAR(p->pos[1], c.uv[1], 1e-6);
        }
    a = box();
    b = box(.35f, .2f);
    ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Union, out, {}, &error)) << error;
    std::map<uint32_t, std::set<std::pair<float, float>>> uv;
    for (const auto& f : out.faces)
        for (const auto& c : f.loop)
            uv[c.vertex].insert({c.uv[0], c.uv[1]});
    bool seam = false;
    for (const auto& [id, values] : uv)
        seam = seam || values.size() > 1;
    EXPECT_TRUE(seam);
}

TEST(MeshBoolean, FixedRotationsSatisfyVolumeIdentities) {
    for (int iteration = 0; iteration < 12; ++iteration) {
        SCOPED_TRACE(iteration);
        auto a = box(), b = box();
        const double angle = (iteration + 1) * .173;
        for (auto& v : b.vertices) {
            const double x = v.pos[0], y = v.pos[1];
            v.pos[0] = float(std::cos(angle) * x - std::sin(angle) * y + .13);
            v.pos[1] = float(std::sin(angle) * x + std::cos(angle) * y + .07);
            v.pos[2] += .11f;
        }
        EditableMesh u, i, d;
        std::string error;
        ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Union, u, {}, &error)) << error;
        ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Intersection, i, {}, &error)) << error;
        ASSERT_TRUE(booleanMeshes(a, b, BooleanOperation::Difference, d, {}, &error)) << error;
        EXPECT_NEAR(volume(a) + volume(b), volume(u) + volume(i), 1e-5);
        EXPECT_NEAR(volume(a), volume(d) + volume(i), 1e-5);
    }
}

TEST(MeshBoolean, PrecisionAndIdExhaustionFailAtomically) {
    auto a = box(), out = box(9);
    const auto revision = out.revision;
    std::string error;
    auto thin = box();
    for (auto& v : thin.vertices)
        v.pos[2] *= 1e-9f;
    EXPECT_FALSE(booleanMeshes(a, thin, BooleanOperation::Union, out, {}, &error));
    for (int kind = 0; kind < 3; ++kind) {
        a = box();
        if (kind == 0)
            a.nextVertexId = UINT32_MAX;
        if (kind == 1)
            a.nextFaceId = UINT32_MAX;
        if (kind == 2)
            a.nextEdgeId = UINT32_MAX;
        EXPECT_FALSE(booleanMeshes(a, box(.5f), BooleanOperation::Union, out, {}, &error));
        EXPECT_EQ(revision, out.revision);
    }
}
