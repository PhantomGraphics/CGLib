#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <set>
#include <vector>

#include "../Core/MeshEditPicking.h"

using namespace Phantom::MeshEdit;

namespace {

// 400x400 viewport looking at the origin from +Z (slightly raised so the top face is also visible), PhantomStudio's
// convention: perspective with the Vulkan Y flip.
MeshPickView frontView(const glm::vec3& eye = glm::vec3(0.f, 0.f, 5.f)) {
    MeshPickView v;
    v.view = glm::lookAt(eye, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    v.proj = glm::perspective(glm::radians(60.f), 1.f, 0.01f, 1000.f);
    v.proj[1][1] *= -1.f;
    v.width = v.height = 400.f;
    return v;
}

glm::vec2 pixelOf(const MeshPickView& v, const EditableMesh& m, uint32_t vertex) {
    glm::vec2 px;
    EXPECT_TRUE(projectToPixels(v, m.findVertex(vertex)->pos, px));
    return px;
}

}  // namespace

TEST(MeshEditPicking, ProjectionPutsTheOriginAtTheCentreAndUpAboveIt) {
    const MeshPickView v = frontView();
    const float origin[3] = { 0, 0, 0 }, above[3] = { 0, 1, 0 }, behind[3] = { 0, 0, 10 };
    glm::vec2 px;
    ASSERT_TRUE(projectToPixels(v, origin, px));
    EXPECT_NEAR(200.f, px.x, 1e-3f);
    EXPECT_NEAR(200.f, px.y, 1e-3f);
    ASSERT_TRUE(projectToPixels(v, above, px));
    EXPECT_LT(px.y, 200.f);  // screen origin is the top-left corner
    EXPECT_FALSE(projectToPixels(v, behind, px));
}

TEST(MeshEditPicking, FrontVertexIsPickedAndTheOneBehindItIsNot) {
    const EditableMesh box = makeEditableBox();
    const MeshPickView v = frontView();
    MeshPickOptions o;
    o.kind = ElementKind::Vertex;
    // Front face (+Z) vertices are 4..7; the back vertices 0..3 project almost to the same pixels (slightly nearer to centre).
    const glm::vec2 front = pixelOf(v, box, 6);
    uint32_t id = 99;
    ASSERT_TRUE(pickMeshElement(box, v, front.x, front.y, o, id));
    EXPECT_EQ(6u, id);

    // Aim at the hidden back vertex 2 from behind the front one: with X-ray it is reachable, without it is hidden.
    const glm::vec2 back = pixelOf(v, box, 2);
    o.vertexRadiusPx = 1.f;
    o.xray = true;
    ASSERT_TRUE(pickMeshElement(box, v, back.x, back.y, o, id));
    EXPECT_EQ(2u, id);
    o.xray = false;
    EXPECT_FALSE(pickMeshElement(box, v, back.x, back.y, o, id));
}

TEST(MeshEditPicking, MissOutsideTheRadius) {
    const EditableMesh box = makeEditableBox();
    const MeshPickView v = frontView();
    MeshPickOptions o;
    uint32_t id = 0;
    EXPECT_FALSE(pickMeshElement(box, v, 5.0, 5.0, o, id));
}

TEST(MeshEditPicking, EdgePicksNearItsMidpointAndFacePicksFrontMost) {
    const EditableMesh box = makeEditableBox();
    const MeshPickView v = frontView();
    // Edge 4-5 (bottom edge of the front face).
    const uint32_t edgeId = box.findEdge(4, 5)->id;
    const glm::vec2 mid = 0.5f * (pixelOf(v, box, 4) + pixelOf(v, box, 5));
    MeshPickOptions o;
    o.kind = ElementKind::Edge;
    uint32_t id = 0;
    ASSERT_TRUE(pickMeshElement(box, v, mid.x, mid.y + 3.0, o, id));
    EXPECT_EQ(edgeId, id);

    o.kind = ElementKind::Face;
    ASSERT_TRUE(pickMeshElement(box, v, 200.0, 200.0, o, id));
    // The +Z face is the one with vertices 4,5,6,7 (loop {4,5,6,7}).
    const EditableFace* f = box.findFace(id);
    ASSERT_NE(nullptr, f);
    std::vector<uint32_t> verts;
    for (const auto& c : f->loop) verts.push_back(c.vertex);
    std::sort(verts.begin(), verts.end());
    EXPECT_EQ((std::vector<uint32_t>{ 4, 5, 6, 7 }), verts);
}

TEST(MeshEditPicking, ModelMatrixIsApplied) {
    const EditableMesh box = makeEditableBox();
    MeshPickView v = frontView();
    v.model = glm::translate(glm::mat4(1.f), glm::vec3(1.f, 0.f, 0.f));
    const glm::vec2 px = pixelOf(v, box, 6);
    EXPECT_GT(px.x, 200.f);
    MeshPickOptions o;
    uint32_t id = 0;
    ASSERT_TRUE(pickMeshElement(box, v, px.x, px.y, o, id));
    EXPECT_EQ(6u, id);
}

TEST(MeshEditPicking, WindowVersusCrossingBoxSelection) {
    const EditableMesh box = makeEditableBox();
    const MeshPickView v = frontView();
    MeshPickOptions o;
    o.xray = true;
    const glm::vec2 c6 = pixelOf(v, box, 6), c4 = pixelOf(v, box, 4);
    const double l = c4.x - 5, r = c6.x + 5, t = c6.y - 5, b = c4.y + 5;  // surrounds the front face only

    o.kind = ElementKind::Vertex;
    auto ids = boxSelectMeshElements(box, v, l, t, r, b, o);
    // The back vertices project inside the front face's outline too (centred perspective), so X-ray picks all 8.
    EXPECT_EQ(8u, ids.size());

    o.xray = false;
    ids = boxSelectMeshElements(box, v, l, t, r, b, o);
    EXPECT_EQ((std::vector<uint32_t>{ 4, 5, 6, 7 }), ids);  // hidden back vertices are skipped

    o.kind = ElementKind::Face;
    o.xray = true;
    ids = boxSelectMeshElements(box, v, l, t, r, b, o);       // window: all four corners inside
    EXPECT_GE(ids.size(), 1u);
    // A small crossing box (right-to-left) around one front corner touches every face meeting there.
    const auto crossing = boxSelectMeshElements(box, v, c6.x + 3, c6.y + 3, c6.x - 3, c6.y - 3, o);
    EXPECT_GE(crossing.size(), 3u);
    // The same box dragged left-to-right is a window: no face fits inside it.
    const auto window = boxSelectMeshElements(box, v, c6.x - 3, c6.y - 3, c6.x + 3, c6.y + 3, o);
    EXPECT_TRUE(window.empty());
}

TEST(MeshEditPicking, EdgesOfTheBoxByCrossingBox) {
    const EditableMesh box = makeEditableBox();
    const MeshPickView v = frontView();
    MeshPickOptions o;
    o.kind = ElementKind::Edge;
    o.xray = true;
    const glm::vec2 mid = 0.5f * (pixelOf(v, box, 4) + pixelOf(v, box, 5));
    const auto hits = boxSelectMeshElements(box, v, mid.x + 2, mid.y + 2, mid.x - 2, mid.y - 2, o);
    EXPECT_NE(std::find(hits.begin(), hits.end(), box.findEdge(4, 5)->id), hits.end());
}
