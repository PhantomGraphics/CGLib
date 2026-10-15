#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>

#include "../MeshEdit/MeshEditOperations.h"

// Pure shape operations and modifiers on an EditableMesh (moved from PhantomStudioTest/MeshEditCommandTest.cpp; the command
// layer's own tests stay in PhantomStudio).
using namespace Phantom::MeshEdit;

namespace {

// Top face of the box is the one whose vertices all have y = +0.5.
uint32_t topFaceId(const EditableMesh& m) {
    for (const auto& f : m.faces) {
        bool top = true;
        for (const auto& c : f.loop) top = top && m.findVertex(c.vertex)->pos[1] > 0.4f;
        if (top) return f.id;
    }
    return ~0u;
}

}  // namespace

namespace {

// Every face of a convex solid must face away from its centre.
void expectOutwardFaces(const EditableMesh& m) {
    double c[3] = { 0, 0, 0 };
    for (const auto& v : m.vertices) for (int k = 0; k < 3; ++k) c[k] += v.pos[k] / m.vertices.size();
    for (const auto& f : m.faces) {
        double n[3] = { 0, 0, 0 }, mid[3] = { 0, 0, 0 };
        for (size_t i = 0; i < f.loop.size(); ++i) {
            const auto* p = m.findVertex(f.loop[i].vertex);
            const auto* q = m.findVertex(f.loop[(i + 1) % f.loop.size()].vertex);
            n[0] += (double(p->pos[1]) - q->pos[1]) * (double(p->pos[2]) + q->pos[2]);
            n[1] += (double(p->pos[2]) - q->pos[2]) * (double(p->pos[0]) + q->pos[0]);
            n[2] += (double(p->pos[0]) - q->pos[0]) * (double(p->pos[1]) + q->pos[1]);
            for (int k = 0; k < 3; ++k) mid[k] += p->pos[k] / f.loop.size();
        }
        EXPECT_GT(n[0] * (mid[0] - c[0]) + n[1] * (mid[1] - c[1]) + n[2] * (mid[2] - c[2]), 0.0) << "face " << f.id;
    }
}

}  // namespace
namespace {

uint32_t edgeBetween(const EditableMesh& m, uint32_t a, uint32_t b) {
    const EditableEdge* e = m.findEdge(a, b);
    return e ? e->id : 0xffffffffu;
}

}  // namespace
namespace {

// A box that is a half: x in [0, 1], so a mirror across x = 0 completes it into a 2-wide box.
EditableMesh halfBox() {
    std::string error;
    EditableMesh half = makeEditableBox();
    std::vector<uint32_t> all;
    for (const auto& v : half.vertices) all.push_back(v.id);
    EditableMesh shifted;
    EXPECT_TRUE(translateVertices(half, all, { 0.5f, 0.f, 0.f }, shifted, &error)) << error;
    return shifted;
}

}  // namespace

TEST(MeshEditOperations, ElementsExpandToVertices) {
    const EditableMesh box = makeEditableBox();
    std::vector<uint32_t> v;
    std::string error;
    ASSERT_TRUE(elementsToVertices(box, ElementKind::Face, { box.faces[0].id }, v, &error)) << error;
    EXPECT_EQ(4u, v.size());
    ASSERT_TRUE(elementsToVertices(box, ElementKind::Edge, { box.edges[0].id }, v, &error));
    EXPECT_EQ(2u, v.size());
    // Two faces sharing an edge: the shared vertices are not counted twice.
    ASSERT_TRUE(elementsToVertices(box, ElementKind::Face, { box.faces[0].id, box.faces[2].id }, v, &error));
    EXPECT_LT(v.size(), 8u);
    EXPECT_FALSE(elementsToVertices(box, ElementKind::Vertex, { 77 }, v, &error));
    EXPECT_FALSE(error.empty());
}
TEST(MeshEditOperations, TranslateKeepsTopologyAndAdvancesRevision) {
    const EditableMesh box = makeEditableBox();
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(translateVertices(box, { 6 }, { 0.f, 1.f, 0.f }, out, &error)) << error;
    EXPECT_FLOAT_EQ(1.5f, out.findVertex(6)->pos[1]);
    EXPECT_FLOAT_EQ(0.5f, box.findVertex(6)->pos[1]);  // input untouched
    EXPECT_GT(out.revision, box.revision);
    EXPECT_EQ(box.edges.size(), out.edges.size());
    for (size_t i = 0; i < box.edges.size(); ++i) EXPECT_EQ(box.edges[i].id, out.edges[i].id);
}
TEST(MeshEditOperations, RotateAndScaleAboutPivot) {
    const EditableMesh box = makeEditableBox();
    std::vector<uint32_t> all;
    for (const auto& v : box.vertices) all.push_back(v.id);
    EditableMesh out;
    std::string error;
    // 90 degrees about Y maps (x,y,z) -> (z,y,-x).
    ASSERT_TRUE(rotateVertices(box, all, { 0.f, 90.f, 0.f }, { 0.f, 0.f, 0.f }, out, &error)) << error;
    const auto& v1 = *box.findVertex(1);  // (+,-,-)
    const auto& r1 = *out.findVertex(1);
    EXPECT_NEAR(v1.pos[2], r1.pos[0], 1e-6f);
    EXPECT_NEAR(-v1.pos[0], r1.pos[2], 1e-6f);
    EXPECT_NEAR(v1.pos[1], r1.pos[1], 1e-6f);

    ASSERT_TRUE(scaleVertices(box, all, { 2.f, 1.f, 1.f }, { 0.f, 0.f, 0.f }, out, &error)) << error;
    EXPECT_FLOAT_EQ(1.0f, out.findVertex(1)->pos[0]);
}
TEST(MeshEditOperations, DegenerateResultIsRejectedWithoutOutput) {
    const EditableMesh box = makeEditableBox();
    std::vector<uint32_t> all;
    for (const auto& v : box.vertices) all.push_back(v.id);
    EditableMesh out;
    out.nextVertexId = 123;  // sentinel
    std::string error;
    EXPECT_FALSE(scaleVertices(box, all, { 0.f, 1.f, 1.f }, { 0.f, 0.f, 0.f }, out, &error));  // flattens every face
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(123u, out.nextVertexId);
    EXPECT_FALSE(translateVertices(box, { 0 }, { std::nanf(""), 0.f, 0.f }, out, &error));
    EXPECT_FALSE(translateVertices(box, {}, { 1.f, 0.f, 0.f }, out, &error));
}
TEST(MeshEditOperations, WorldVectorUsesInverseRotationAndScale) {
    std::array<float, 3> local{};
    std::string error;
    // Object rotated 90 degrees about Y with scale 2: world +X is local -Z-ish; check by round trip.
    ASSERT_TRUE(worldVectorToLocal({ 0.f, 90.f, 0.f }, { 2.f, 2.f, 2.f }, { 2.f, 0.f, 0.f }, local, &error)) << error;
    // R*(S*local) must reproduce the world vector.
    const float sx = local[0] * 2.f, sy = local[1] * 2.f, sz = local[2] * 2.f;
    EXPECT_NEAR(2.f, sz * 1.f + sx * 0.f, 1e-5f);   // Ry(90): x' = z, z' = -x  =>  world.x = sz
    EXPECT_NEAR(0.f, -sx, 1e-5f);                   //                           world.z = -sx
    EXPECT_NEAR(0.f, sy, 1e-5f);
    EXPECT_FALSE(worldVectorToLocal({ 0.f, 0.f, 0.f }, { 1.f, 0.f, 1.f }, { 1.f, 0.f, 0.f }, local, &error));
}
TEST(MeshExtrudeOperation, BoxTopFaceGrowsAPrism) {
    const EditableMesh box = makeEditableBox();
    const uint32_t top = topFaceId(box);
    EditableMesh out;
    std::string error;
    size_t sides = 0;
    ASSERT_TRUE(extrudeFaces(box, { top }, 0.5f, out, &error, &sides)) << error;
    EXPECT_EQ(4u, sides);
    EXPECT_EQ(12u, out.vertices.size());   // 4 boundary vertices copied
    EXPECT_EQ(10u, out.faces.size());
    EXPECT_EQ(20u, out.edges.size());      // Euler: V - E + F = 2
    EXPECT_GT(out.revision, box.revision);
    // The region keeps its face id and now sits at y = 1.
    const EditableFace* moved = out.findFace(top);
    ASSERT_NE(nullptr, moved);
    for (const auto& c : moved->loop) EXPECT_FLOAT_EQ(1.0f, out.findVertex(c.vertex)->pos[1]);
    // The old vertices stay where they were (the wall bottoms).
    for (uint32_t v = 0; v < 8; ++v) EXPECT_FLOAT_EQ(box.findVertex(v)->pos[1], out.findVertex(v)->pos[1]);
    expectOutwardFaces(out);
    EXPECT_TRUE(out.validate(&error)) << error;
    // Side wall UVs: u spans the edge length, v the distance.
    const EditableFace& wall = out.faces.back();
    EXPECT_FLOAT_EQ(1.0f, wall.loop[1].uv[0]);
    EXPECT_FLOAT_EQ(0.5f, wall.loop[2].uv[1]);
    // Input untouched.
    EXPECT_EQ(8u, box.vertices.size());
}
TEST(MeshExtrudeOperation, InteriorVerticesMoveInsteadOfDuplicating) {
    const EditableMesh plane = makeEditablePlane(1.0f, 2);  // 3x3 vertices, 4 quads
    std::vector<uint32_t> all;
    for (const auto& f : plane.faces) all.push_back(f.id);
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(extrudeFaces(plane, all, 1.0f, out, &error)) << error;
    EXPECT_EQ(9u + 8u, out.vertices.size());  // 8 boundary vertices copied, the centre one moved
    EXPECT_EQ(4u + 8u, out.faces.size());
    EXPECT_FLOAT_EQ(1.0f, out.findVertex(4)->pos[1]);
    EXPECT_TRUE(out.validate(&error)) << error;
}
TEST(MeshExtrudeOperation, RejectsWhatItCannotExtrude) {
    const EditableMesh box = makeEditableBox();
    EditableMesh out;
    std::string error;
    std::vector<uint32_t> everything;
    for (const auto& f : box.faces) everything.push_back(f.id);
    EXPECT_FALSE(extrudeFaces(box, everything, 1.0f, out, &error));  // closed surface: no boundary
    EXPECT_FALSE(extrudeFaces(box, {}, 1.0f, out, &error));
    EXPECT_FALSE(extrudeFaces(box, { 99 }, 1.0f, out, &error));
    EXPECT_FALSE(extrudeFaces(box, { 0 }, 0.f, out, &error));
    EXPECT_FALSE(extrudeFaces(box, { 0 }, std::nanf(""), out, &error));
    EXPECT_FALSE(extrudeFaces(box, { 0, 2 }, 1.0f, out, &error));    // adjacent but not coplanar
    EXPECT_NE(std::string::npos, error.find("coplanar"));
    EXPECT_FALSE(extrudeFaces(box, { 0, 1 }, 1.0f, out, &error));    // opposite faces: not connected
    EXPECT_NE(std::string::npos, error.find("connected"));
}
TEST(MeshExtrudeOperation, NegativeDistanceDigsIn) {
    const EditableMesh box = makeEditableBox();
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(extrudeFaces(box, { topFaceId(box) }, -0.25f, out, &error)) << error;
    for (const auto& c : out.findFace(topFaceId(box))->loop) EXPECT_FLOAT_EQ(0.25f, out.findVertex(c.vertex)->pos[1]);
    // Walls face the pocket: the wall on the -X side has normal +X.
    const EditableFace& wall = out.faces[6];
    float n[3] = { 0, 0, 0 };
    for (size_t i = 0; i < wall.loop.size(); ++i) {
        const auto* p = out.findVertex(wall.loop[i].vertex);
        const auto* q = out.findVertex(wall.loop[(i + 1) % wall.loop.size()].vertex);
        n[0] += (p->pos[1] - q->pos[1]) * (p->pos[2] + q->pos[2]);
        n[1] += (p->pos[2] - q->pos[2]) * (p->pos[0] + q->pos[0]);
        n[2] += (p->pos[0] - q->pos[0]) * (p->pos[1] + q->pos[1]);
    }
    const auto* a = out.findVertex(wall.loop[0].vertex);
    // Pointing toward the box axis (x = z = 0) from the wall's own position.
    EXPECT_LT(n[0] * a->pos[0] + n[2] * a->pos[2], 0.0f);
}
TEST(MeshInsetOperation, BoxTopFaceGetsARingAndStaysFlat) {
    const EditableMesh box = makeEditableBox();
    const uint32_t top = topFaceId(box);
    EditableMesh out;
    std::string error;
    size_t ring = 0;
    ASSERT_TRUE(insetFaces(box, { top }, 0.1f, 0.f, out, &error, &ring)) << error;
    EXPECT_EQ(4u, ring);
    EXPECT_EQ(12u, out.vertices.size());
    EXPECT_EQ(10u, out.faces.size());
    EXPECT_EQ(20u, out.edges.size());
    // The inner face keeps its id, stays at y = 0.5 and shrinks to a 0.8 square.
    const EditableFace* inner = out.findFace(top);
    ASSERT_NE(nullptr, inner);
    for (const auto& c : inner->loop) {
        const auto* v = out.findVertex(c.vertex);
        EXPECT_FLOAT_EQ(0.5f, v->pos[1]);
        EXPECT_NEAR(0.4f, std::fabs(v->pos[0]), 1e-6f);
        EXPECT_NEAR(0.4f, std::fabs(v->pos[2]), 1e-6f);
    }
    // The old corners stay (outer edge of the ring); every ring quad is coplanar and faces up like the top.
    for (uint32_t v = 0; v < 8; ++v) EXPECT_FLOAT_EQ(box.findVertex(v)->pos[1], out.findVertex(v)->pos[1]);
    for (size_t i = 6; i < out.faces.size(); ++i) {
        const auto& f = out.faces[i];
        for (const auto& c : f.loop) EXPECT_FLOAT_EQ(0.5f, out.findVertex(c.vertex)->pos[1]);
    }
    expectOutwardFaces(out);
    EXPECT_TRUE(out.validate(&error)) << error;
}
TEST(MeshInsetOperation, DepthSinksTheInnerRegion) {
    const EditableMesh box = makeEditableBox();
    const uint32_t top = topFaceId(box);
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(insetFaces(box, { top }, 0.2f, -0.1f, out, &error)) << error;
    for (const auto& c : out.findFace(top)->loop) EXPECT_FLOAT_EQ(0.4f, out.findVertex(c.vertex)->pos[1]);
    EXPECT_TRUE(out.validate(&error)) << error;
}
TEST(MeshInsetOperation, RegionInsetMovesOnlyTheBoundary) {
    const EditableMesh plane = makeEditablePlane(1.0f, 2);  // 3x3 vertices, 4 quads
    std::vector<uint32_t> all;
    for (const auto& f : plane.faces) all.push_back(f.id);
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(insetFaces(plane, all, 0.25f, 0.f, out, &error)) << error;
    EXPECT_EQ(9u + 8u, out.vertices.size());
    EXPECT_EQ(4u + 8u, out.faces.size());
    EXPECT_FLOAT_EQ(plane.findVertex(4)->pos[0], out.findVertex(4)->pos[0]);  // the centre does not move
    // A boundary vertex of the 2x2 plane (corner at |x| = |z| = 1) moves to |x| = |z| = 0.75.
    float maxAbs = 0.f;
    for (const auto& f : out.faces)
        if (std::find(all.begin(), all.end(), f.id) != all.end())
            for (const auto& c : f.loop) maxAbs = std::max(maxAbs, std::fabs(out.findVertex(c.vertex)->pos[0]));
    EXPECT_NEAR(0.75f, maxAbs, 1e-6f);
    EXPECT_TRUE(out.validate(&error)) << error;
}
TEST(MeshInsetOperation, RejectsWhatItCannotInset) {
    const EditableMesh box = makeEditableBox();
    const uint32_t top = topFaceId(box);
    EditableMesh out;
    std::string error;
    EXPECT_FALSE(insetFaces(box, { top }, 0.f, 0.f, out, &error));
    EXPECT_FALSE(insetFaces(box, { top }, -0.1f, 0.f, out, &error));
    EXPECT_FALSE(insetFaces(box, { top }, std::nanf(""), 0.f, out, &error));
    EXPECT_FALSE(insetFaces(box, { top }, 0.1f, std::nanf(""), out, &error));
    EXPECT_FALSE(insetFaces(box, { top }, 0.6f, 0.f, out, &error));  // beyond the inradius: would invert the face
    EXPECT_NE(std::string::npos, error.find("too large"));
    EXPECT_FALSE(insetFaces(box, { 0, 2 }, 0.1f, 0.f, out, &error)); // not coplanar
    EXPECT_FALSE(insetFaces(box, {}, 0.1f, 0.f, out, &error));
    EXPECT_EQ(6u, box.faces.size());
}
TEST(MeshBevelOperation, CubeEdgeGetsAFlatChamfer) {
    const EditableMesh box = makeEditableBox();
    const uint32_t edge = edgeBetween(box, 2, 6);  // top, +X: both faces are quads
    ASSERT_NE(0xffffffffu, edge);
    EditableMesh out;
    std::string error;
    std::vector<uint32_t> bevel;
    ASSERT_TRUE(bevelEdges(box, { edge }, 0.1f, out, &error, &bevel)) << error;
    ASSERT_EQ(1u, bevel.size());
    EXPECT_EQ(10u, out.vertices.size());   // 2 endpoints replaced by 4
    EXPECT_EQ(9u, out.faces.size());       // 6 + bevel + 2 split end faces
    EXPECT_EQ(17u, out.edges.size());      // Euler: V - E + F = 2
    expectOutwardFaces(out);
    EXPECT_TRUE(out.validate(&error)) << error;
    // The bevel quad is flat and its two long edges sit 0.1 away from the old edge on each face.
    const EditableFace* quad = out.findFace(bevel[0]);
    ASSERT_NE(nullptr, quad);
    double cy = 0, cx = 0;
    for (const auto& c : quad->loop) { cy += out.findVertex(c.vertex)->pos[1] / 4; cx += out.findVertex(c.vertex)->pos[0] / 4; }
    EXPECT_NEAR(0.45, cy, 1e-6);
    EXPECT_NEAR(0.45, cx, 1e-6);
    EXPECT_EQ(nullptr, out.findVertex(2));  // the old endpoints are gone
    EXPECT_EQ(nullptr, out.findVertex(6));
    EXPECT_EQ(8u, box.vertices.size());     // input untouched
}
TEST(MeshBevelOperation, SeparateEdgesAndRejections) {
    const EditableMesh box = makeEditableBox();
    EditableMesh out;
    std::string error;
    // Two opposite top edges (+X and -X) share no vertex.
    ASSERT_TRUE(bevelEdges(box, { edgeBetween(box, 2, 6), edgeBetween(box, 3, 7) }, 0.1f, out, &error)) << error;
    EXPECT_TRUE(out.validate(&error)) << error;
    expectOutwardFaces(out);
    EXPECT_FALSE(bevelEdges(box, { edgeBetween(box, 2, 6), edgeBetween(box, 6, 5) }, 0.1f, out, &error));  // share vertex 6
    EXPECT_NE(std::string::npos, error.find("share"));
    EXPECT_FALSE(bevelEdges(box, { edgeBetween(box, 2, 6) }, 0.f, out, &error));
    EXPECT_FALSE(bevelEdges(box, { edgeBetween(box, 2, 6) }, -0.1f, out, &error));
    EXPECT_FALSE(bevelEdges(box, { edgeBetween(box, 2, 6) }, 1.0f, out, &error));   // longer than the cube side
    EXPECT_NE(std::string::npos, error.find("too large"));
    EXPECT_FALSE(bevelEdges(box, {}, 0.1f, out, &error));
    EXPECT_FALSE(bevelEdges(box, { 9999 }, 0.1f, out, &error));
    const EditableMesh plane = makeEditablePlane(1.0f, 1);
    EXPECT_FALSE(bevelEdges(plane, { plane.edges[0].id }, 0.1f, out, &error));      // border edge
}
TEST(MeshLoopCutOperation, BoxRingGetsFourNewVerticesAndFourSplitFaces) {
    const EditableMesh box = makeEditableBox();
    const uint32_t edge = edgeBetween(box, 2, 6);  // top, +X: the ring runs round the other four faces
    EditableMesh out;
    std::string error;
    std::vector<uint32_t> created;
    size_t split = 0;
    ASSERT_TRUE(loopCut(box, edge, 0.5f, out, &error, &created, &split)) << error;
    EXPECT_EQ(4u, created.size());
    EXPECT_EQ(4u, split);
    EXPECT_EQ(12u, out.vertices.size());
    EXPECT_EQ(10u, out.faces.size());
    EXPECT_EQ(20u, out.edges.size());      // Euler: V - E + F = 2
    // The ring lies in the plane z = 0: it cuts the four box edges parallel to z at their middle.
    for (uint32_t id : created) EXPECT_NEAR(0.0, out.findVertex(id)->pos[2], 1e-6);
    expectOutwardFaces(out);
    EXPECT_TRUE(out.validate(&error)) << error;
    EXPECT_EQ(8u, box.vertices.size());    // input untouched
}
TEST(MeshLoopCutOperation, FactorMovesTheLoopAndOpenRingsStopAtTheBorder) {
    const EditableMesh box = makeEditableBox();
    EditableMesh out;
    std::string error;
    std::vector<uint32_t> created;
    ASSERT_TRUE(loopCut(box, edgeBetween(box, 2, 6), 0.25f, out, &error, &created)) << error;
    double sum = 0;
    for (uint32_t id : created) sum += out.findVertex(id)->pos[2];
    EXPECT_NEAR(4 * 0.25, std::fabs(sum), 1e-5);  // all four sit 0.25 of the way from the same side (z = -0.25 or +0.25)
    const EditableMesh plane = makeEditablePlane(1.0f, 2);  // 3x3 vertices, 4 quads
    const EditableEdge* inner = plane.findEdge(1, 4);        // from the border into the centre
    ASSERT_NE(nullptr, inner);
    size_t split = 0;
    ASSERT_TRUE(loopCut(plane, inner->id, 0.5f, out, &error, &created, &split)) << error;
    EXPECT_EQ(3u, created.size());
    EXPECT_EQ(2u, split);
    EXPECT_EQ(12u, out.vertices.size());
    EXPECT_EQ(6u, out.faces.size());
    EXPECT_TRUE(out.validate(&error)) << error;
}
TEST(MeshLoopCutOperation, RejectsWhatItCannotCut) {
    const EditableMesh box = makeEditableBox();
    EditableMesh out;
    std::string error;
    const uint32_t edge = edgeBetween(box, 2, 6);
    EXPECT_FALSE(loopCut(box, edge, 0.f, out, &error));
    EXPECT_FALSE(loopCut(box, edge, 1.f, out, &error));
    EXPECT_FALSE(loopCut(box, edge, std::nanf(""), out, &error));
    EXPECT_FALSE(loopCut(box, 9999, 0.5f, out, &error));
    // A triangle in the ring would leave a T-junction.
    EditableMesh tri = makeEditablePlane(1.0f, 1);
    tri.faces[0].loop.erase(tri.faces[0].loop.begin() + 3);
    tri.rebuildEdges();
    EXPECT_FALSE(loopCut(tri, tri.edges[0].id, 0.5f, out, &error));
    EXPECT_NE(std::string::npos, error.find("quad"));
}
TEST(MeshShadeOperation, SmoothAveragesAtSharedVerticesAndFlatRestores) {
    const EditableMesh box = makeEditableBox();
    std::vector<uint32_t> all;
    for (const auto& f : box.faces) all.push_back(f.id);
    EditableMesh smooth, flat;
    std::string error;
    size_t changed = 0;
    ASSERT_TRUE(shadeFaces(box, all, true, 180.f, smooth, &error, &changed)) << error;
    EXPECT_EQ(24u, changed);  // every corner of the six quads
    const double s = 1.0 / std::sqrt(3.0);
    for (const auto& f : smooth.faces)
        for (const auto& c : f.loop) {
            ASSERT_TRUE(c.hasNormal);
            const auto* v = smooth.findVertex(c.vertex);
            EXPECT_NEAR(s * (v->pos[0] > 0 ? 1 : -1), c.normal[0], 1e-5);
            EXPECT_NEAR(s * (v->pos[1] > 0 ? 1 : -1), c.normal[1], 1e-5);
            EXPECT_NEAR(s * (v->pos[2] > 0 ? 1 : -1), c.normal[2], 1e-5);
        }
    ASSERT_TRUE(shadeFaces(smooth, all, false, 180.f, flat, &error, &changed)) << error;
    EXPECT_EQ(24u, changed);
    for (const auto& f : flat.faces) for (const auto& c : f.loop) EXPECT_FALSE(c.hasNormal);
    EXPECT_FALSE(box.faces[0].loop[0].hasNormal);  // input untouched
}
TEST(MeshShadeOperation, AngleKeepsCreasesAndOnlySelectedFacesChange) {
    const EditableMesh box = makeEditableBox();
    std::vector<uint32_t> all;
    for (const auto& f : box.faces) all.push_back(f.id);
    EditableMesh out;
    std::string error;
    size_t changed = 99;
    ASSERT_TRUE(shadeFaces(box, all, true, 30.f, out, &error, &changed)) << error;   // 90-degree creases stay hard
    EXPECT_EQ(0u, changed);
    for (const auto& f : out.faces) for (const auto& c : f.loop) EXPECT_FALSE(c.hasNormal);
    ASSERT_TRUE(shadeFaces(box, { topFaceId(box) }, true, 180.f, out, &error, &changed)) << error;
    EXPECT_EQ(4u, changed);
    for (const auto& f : out.faces)
        for (const auto& c : f.loop) EXPECT_EQ(f.id == topFaceId(box), c.hasNormal);
}
TEST(MeshShadeOperation, Rejections) {
    const EditableMesh box = makeEditableBox();
    EditableMesh out;
    std::string error;
    EXPECT_FALSE(shadeFaces(box, {}, true, 180.f, out, &error));
    EXPECT_FALSE(shadeFaces(box, { 99 }, true, 180.f, out, &error));
    EXPECT_FALSE(shadeFaces(box, { 0 }, true, -1.f, out, &error));
    EXPECT_FALSE(shadeFaces(box, { 0 }, true, 181.f, out, &error));
    EXPECT_FALSE(shadeFaces(box, { 0 }, true, std::nanf(""), out, &error));
}
TEST(MeshMirrorOperation, HalfBoxBecomesAWholeBoxWithAWeldedSeam) {
    EditableMesh half = halfBox();       // x in [0, 1]: the four x = 0 vertices lie on the plane
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(mirrorMesh(half, { true, false, false }, 0.001f, out, &error)) << error;
    // 8 vertices + 4 mirrored (the plane vertices are shared); the cap face in the plane is removed, 5 + 5 faces remain.
    EXPECT_EQ(12u, out.vertices.size());
    EXPECT_EQ(10u, out.faces.size());
    EXPECT_EQ(20u, out.edges.size());    // a closed box with a ring of seam vertices: V - E + F = 2
    EXPECT_TRUE(out.validate(&error)) << error;
    float minX = 1e9f, maxX = -1e9f;
    for (const auto& v : out.vertices) { minX = std::min(minX, v.pos[0]); maxX = std::max(maxX, v.pos[0]); }
    EXPECT_FLOAT_EQ(-1.0f, minX);
    EXPECT_FLOAT_EQ(1.0f, maxX);
    EXPECT_EQ(8u, half.vertices.size());  // input untouched
}
TEST(MeshMirrorOperation, RejectsBadArgumentsAndOpenSeams) {
    EditableMesh out;
    std::string error;
    const EditableMesh half = halfBox();
    EXPECT_FALSE(mirrorMesh(half, { false, false, false }, 0.001f, out, &error));
    EXPECT_FALSE(mirrorMesh(half, { true, false, false }, -1.f, out, &error));
    EXPECT_FALSE(mirrorMesh(half, { true, false, false }, std::nanf(""), out, &error));
    // A mesh entirely in the plane leaves nothing.
    EXPECT_FALSE(mirrorMesh(makeEditablePlane(1.0f, 1), { false, true, false }, 0.001f, out, &error));
    EXPECT_NE(std::string::npos, error.find("no faces"));
}
TEST(MeshSubdivisionOperation, BoxBecomesAClosedQuadMeshThatShrinksTowardsASphere) {
    const EditableMesh box = makeEditableBox();
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(subdivideMesh(box, 1, true, out, &error)) << error;
    EXPECT_EQ(8u + 12u + 6u, out.vertices.size());   // corners + edge points + face points
    EXPECT_EQ(24u, out.faces.size());                // 6 quads x 4
    EXPECT_EQ(48u, out.edges.size());                // V - E + F = 26 - 48 + 24 = 2
    for (const auto& f : out.faces) EXPECT_EQ(4u, f.loop.size());
    expectOutwardFaces(out);
    EXPECT_TRUE(out.validate(&error)) << error;
    // Catmull-Clark pulls the corners inward: the standard rule moves (0.5,0.5,0.5) to (5/18-ish) -> strictly smaller than 0.5.
    float maxAbs = 0.f;
    for (const auto& v : out.vertices) for (int k = 0; k < 3; ++k) maxAbs = std::max(maxAbs, std::fabs(v.pos[k]));
    EXPECT_NEAR(0.5f, maxAbs, 1e-6f);                // the face points / edge points stay on the cube faces
    const EditableVertex* corner = out.findVertex(6);
    ASSERT_NE(nullptr, corner);
    EXPECT_LT(std::fabs(corner->pos[0]), 0.5f);
    EXPECT_NEAR(0.2778f, corner->pos[0], 1e-3f);     // (F + 2R + 0*P)/3 for the cube corner
    // Smooth shading stores normals, faceted does not.
    bool anyNormal = false;
    for (const auto& f : out.faces) for (const auto& c : f.loop) anyNormal = anyNormal || c.hasNormal;
    EXPECT_TRUE(anyNormal);
    EditableMesh faceted;
    ASSERT_TRUE(subdivideMesh(box, 1, false, faceted, &error)) << error;
    for (const auto& f : faceted.faces) for (const auto& c : f.loop) EXPECT_FALSE(c.hasNormal);
    EXPECT_EQ(6u, box.faces.size());                 // input untouched
}
TEST(MeshSubdivisionOperation, LevelsOpenBordersAndRejections) {
    EditableMesh out;
    std::string error;
    const EditableMesh plane = makeEditablePlane(1.0f, 1);   // one open quad
    ASSERT_TRUE(subdivideMesh(plane, 2, true, out, &error)) << error;
    EXPECT_EQ(16u, out.faces.size());
    EXPECT_TRUE(out.validate(&error)) << error;
    // The border crease rule keeps the outline: it still spans [-1, 1] in x and z... corners move inward only along the border.
    for (const auto& v : out.vertices) EXPECT_FLOAT_EQ(0.f, v.pos[1]);
    ASSERT_TRUE(subdivideMesh(makeEditableBox(), 3, true, out, &error)) << error;
    EXPECT_EQ(6u * 64u, out.faces.size());
    EXPECT_FALSE(subdivideMesh(plane, 0, true, out, &error));
    EXPECT_FALSE(subdivideMesh(plane, 5, true, out, &error));
    EditableMesh empty;
    EXPECT_FALSE(subdivideMesh(empty, 1, true, out, &error));
    // A triangle gives three quads.
    EditableMesh tri = makeEditablePlane(1.0f, 1);
    tri.faces[0].loop.erase(tri.faces[0].loop.begin() + 3);
    tri.rebuildEdges();
    ASSERT_TRUE(subdivideMesh(tri, 1, false, out, &error)) << error;
    EXPECT_EQ(3u, out.faces.size());
}

// ---- merge by distance ----

TEST(MergeByDistance, WeldsAThinExtrusionBackIntoTheBox) {
    EditableMesh box = makeEditableBox(0.5f);
    EditableMesh thick;
    ASSERT_TRUE(extrudeFaces(box, { topFaceId(box) }, 0.0005f, thick));
    ASSERT_GT(thick.vertices.size(), box.vertices.size());
    EditableMesh welded;
    size_t merged = 0, skipped = 0;
    std::string error;
    ASSERT_TRUE(mergeByDistance(thick, {}, 0.001f, welded, &error, &merged, &skipped)) << error;
    EXPECT_EQ(box.vertices.size(), welded.vertices.size());
    EXPECT_EQ(box.faces.size(), welded.faces.size());
    EXPECT_EQ(0u, skipped);
    EXPECT_GE(merged, 8u);
    EXPECT_TRUE(welded.validate(&error)) << error;
}

TEST(MergeByDistance, OnlyTheGivenVerticesAreCandidatesAndNothingToMergeFails) {
    EditableMesh box = makeEditableBox(0.5f);
    EditableMesh thick;
    ASSERT_TRUE(extrudeFaces(box, { topFaceId(box) }, 0.0005f, thick));
    std::string error;
    EditableMesh out;
    // A box corner pair is 1 apart: a tiny distance finds nothing.
    EXPECT_FALSE(mergeByDistance(box, {}, 0.001f, out, &error));
    EXPECT_FALSE(error.empty());
    // Restricted to one original vertex only, there is no partner among the candidates.
    EXPECT_FALSE(mergeByDistance(thick, { thick.vertices.front().id }, 0.001f, out, &error));
    EXPECT_FALSE(mergeByDistance(thick, {}, -1.f, out, &error));
    // A large distance welds the whole box into a point: refused rather than producing an invalid mesh.
    EXPECT_FALSE(mergeByDistance(box, {}, 5.f, out, &error));
}

// ---- bridge ----

namespace {

// Two unit boxes stacked with a gap: A centred at y = 0, B at y = 2.
EditableMesh twoBoxes() {
    EditableMesh m = makeEditableBox(0.5f);
    const EditableMesh other = makeEditableBox(0.5f);
    std::map<uint32_t, uint32_t> remap;
    for (const auto& v : other.vertices) remap[v.id] = m.addVertex(v.pos[0], v.pos[1] + 2.f, v.pos[2]);
    for (const auto& f : other.faces) {
        std::vector<EditableCorner> loop = f.loop;
        for (auto& c : loop) c.vertex = remap[c.vertex];
        m.addFace(loop);
    }
    m.rebuildEdges();
    return m;
}

// The face whose vertices all lie at height y (within 0.01).
uint32_t faceAtHeight(const EditableMesh& m, float y) {
    for (const auto& f : m.faces) {
        bool all = true;
        for (const auto& c : f.loop) all = all && std::fabs(m.findVertex(c.vertex)->pos[1] - y) < 0.01f;
        if (all) return f.id;
    }
    return ~0u;
}

// Every edge of a closed manifold is used by exactly two faces.
bool isClosed(const EditableMesh& m) {
    std::map<std::pair<uint32_t, uint32_t>, int> uses;
    for (const auto& f : m.faces)
        for (size_t i = 0; i < f.loop.size(); ++i) {
            const uint32_t p = f.loop[i].vertex, q = f.loop[(i + 1) % f.loop.size()].vertex;
            ++uses[{ std::min(p, q), std::max(p, q) }];
        }
    for (const auto& u : uses) if (u.second != 2) return false;
    return true;
}

}  // namespace

TEST(BridgeFaces, JoinsTwoBoxesIntoOneClosedSolid) {
    const EditableMesh boxes = twoBoxes();
    ASSERT_TRUE(isClosed(boxes));
    EditableMesh out;
    std::vector<uint32_t> created;
    std::string error;
    ASSERT_TRUE(bridgeFaces(boxes, { faceAtHeight(boxes, 0.5f), faceAtHeight(boxes, 1.5f) }, out, &error, &created)) << error;
    EXPECT_EQ(12u - 2u + 4u, out.faces.size());
    EXPECT_EQ(4u, created.size());
    for (uint32_t id : created) EXPECT_EQ(4u, out.findFace(id)->loop.size());
    EXPECT_EQ(boxes.vertices.size(), out.vertices.size());
    EXPECT_TRUE(isClosed(out));
    EXPECT_TRUE(out.validate(&error)) << error;
    // Each wall faces away from the neck axis (x = z = 0).
    for (uint32_t id : created) {
        const EditableFace& f = *out.findFace(id);
        float n[3];
        ASSERT_GT(newellNormal(out, f, n), 0.f);
        float mid[2] = { 0.f, 0.f };
        for (const auto& c : f.loop) { mid[0] += out.findVertex(c.vertex)->pos[0] / 4.f; mid[1] += out.findVertex(c.vertex)->pos[2] / 4.f; }
        EXPECT_GT(n[0] * mid[0] + n[2] * mid[1], 0.f) << "wall " << id;
    }
}

TEST(BridgeFaces, RejectsBadFaceSelections) {
    const EditableMesh boxes = twoBoxes();
    EditableMesh out;
    std::string error;
    const uint32_t top = faceAtHeight(boxes, 0.5f);
    EXPECT_FALSE(bridgeFaces(boxes, { top }, out, &error));                                       // one face
    EXPECT_FALSE(bridgeFaces(boxes, { top, 9999 }, out, &error));                                 // missing face
    // Top and bottom of the same box: the new walls would double the box's own sides (three faces on one edge).
    EXPECT_FALSE(bridgeFaces(boxes, { top, faceAtHeight(boxes, -0.5f) }, out, &error));
    EXPECT_FALSE(error.empty());
}

TEST(BridgeEdges, JoinsTwoOpenLoopsAndRejectsMismatches) {
    EditableMesh boxes = twoBoxes();
    // Remove the facing faces so both rims become boundary loops.
    EditableMesh holes;
    ASSERT_TRUE(deleteElements(boxes, ElementKind::Face, { faceAtHeight(boxes, 0.5f), faceAtHeight(boxes, 1.5f) }, holes));
    std::vector<uint32_t> rim;
    for (const auto& e : holes.edges) {
        const float ya = holes.findVertex(e.a)->pos[1], yb = holes.findVertex(e.b)->pos[1];
        if (std::fabs(ya - yb) < 1e-6f && (std::fabs(ya - 0.5f) < 1e-6f || std::fabs(ya - 1.5f) < 1e-6f)) rim.push_back(e.id);
    }
    ASSERT_EQ(8u, rim.size());
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(bridgeEdges(holes, rim, out, &error)) << error;
    EXPECT_TRUE(isClosed(out));

    // One loop only, an interior edge, and unequal chains are refused.
    std::vector<uint32_t> oneLoop(rim.begin(), rim.begin() + 4);
    EXPECT_FALSE(bridgeEdges(holes, oneLoop, out, &error));
    // Two edges that are not on the boundary (the box's vertical edges).
    std::vector<uint32_t> interior;
    for (const auto& e : holes.edges)
        if (std::fabs(holes.findVertex(e.a)->pos[1] - holes.findVertex(e.b)->pos[1]) > 0.5f && interior.size() < 2) interior.push_back(e.id);
    EXPECT_FALSE(bridgeEdges(holes, interior, out, &error));
    std::vector<uint32_t> unequal(rim.begin(), rim.begin() + 7);
    EXPECT_FALSE(bridgeEdges(holes, unequal, out, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(bridgeEdges(holes, { 99999, 99998 }, out, &error));
}

// ---- knife ----

TEST(KnifePlane, HorizontalCutSplitsTheFourSidesIntoQuads) {
    const EditableMesh box = makeEditableBox(0.5f);
    EditableMesh out;
    std::vector<uint32_t> cut;
    size_t split = 0;
    std::string error;
    ASSERT_TRUE(knifePlane(box, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, out, &error, &cut, &split)) << error;
    EXPECT_EQ(12u, out.vertices.size());
    EXPECT_EQ(10u, out.faces.size());
    EXPECT_EQ(4u, split);
    EXPECT_EQ(4u, cut.size());
    for (uint32_t id : cut) EXPECT_NEAR(0.f, out.findVertex(id)->pos[1], 1.0e-6f);
    for (const auto& f : out.faces) EXPECT_EQ(4u, f.loop.size());
    EXPECT_TRUE(out.validate(&error)) << error;
}

TEST(KnifePlane, ADiagonalThroughCornersMakesTrianglesWithoutNewVertices) {
    const EditableMesh box = makeEditableBox(0.5f);
    EditableMesh out;
    std::string error;
    size_t split = 0;
    ASSERT_TRUE(knifePlane(box, { 0.f, 0.f, 0.f }, { 1.f, 1.f, 0.f }, out, &error, nullptr, &split)) << error;
    EXPECT_EQ(8u, out.vertices.size()) << "the plane passes through existing vertices";
    EXPECT_EQ(2u, split);
    EXPECT_EQ(8u, out.faces.size());
    EXPECT_TRUE(out.validate(&error)) << error;
}

TEST(KnifePlane, ACornerCutKeepsEveryFaceATriangleOrQuad) {
    const EditableMesh box = makeEditableBox(0.5f);
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(knifePlane(box, { 0.3f, 0.3f, 0.3f }, { 1.f, 1.f, 1.f }, out, &error)) << error;
    EXPECT_EQ(11u, out.vertices.size());
    EXPECT_EQ(12u, out.faces.size());
    for (const auto& f : out.faces) EXPECT_TRUE(f.loop.size() == 3 || f.loop.size() == 4);
    EXPECT_TRUE(out.validate(&error)) << error;
    // Euler's formula for a closed surface still holds.
    EXPECT_EQ(2, int(out.vertices.size()) - int(out.edges.size()) + int(out.faces.size()));
}

TEST(KnifePlane, UvIsInterpolatedAndNothingToCutIsRefused) {
    const EditableMesh box = makeEditableBox(0.5f);
    EditableMesh out;
    std::string error;
    ASSERT_TRUE(knifePlane(box, { 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, out, &error)) << error;
    // Every corner on the cut lies between its neighbours' UVs: all UVs stay inside the box's UV range.
    float lo = 1e9f, hi = -1e9f;
    for (const auto& f : box.faces) for (const auto& c : f.loop) { lo = std::min({ lo, c.uv[0], c.uv[1] }); hi = std::max({ hi, c.uv[0], c.uv[1] }); }
    for (const auto& f : out.faces) for (const auto& c : f.loop) { EXPECT_GE(c.uv[0], lo - 1e-5f); EXPECT_LE(c.uv[0], hi + 1e-5f); EXPECT_GE(c.uv[1], lo - 1e-5f); EXPECT_LE(c.uv[1], hi + 1e-5f); }

    EditableMesh untouched;
    EXPECT_FALSE(knifePlane(box, { 0.f, 5.f, 0.f }, { 0.f, 1.f, 0.f }, untouched, &error)) << "a plane that misses the mesh";
    EXPECT_FALSE(knifePlane(box, { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f }, untouched, &error)) << "a zero normal";
    EXPECT_FALSE(knifePlane(box, { 0.f, 0.5f, 0.f }, { 0.f, 1.f, 0.f }, untouched, &error)) << "a plane along the top face touches no face on both sides";
}
