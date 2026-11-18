#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "../Core/RigPick.h"

using namespace Phantom::Animation;
namespace RS = Phantom::Animation::RigSkinning;
namespace RP = Phantom::Animation::RigPick;

namespace {

SkinVertex vertexAt(const glm::vec3& p)
{
    SkinVertex v;
    v.position = p;
    return v;
}

// A strip of 4 quads along Y, 1 unit wide, vertices duplicated per quad. An orthographic camera maps x 0..2, y 0..2 onto a 200 x 200 viewport.
struct PickFixture {
    AuthoredRig rig;
    uint32_t a = 0, b = 0;
    RigMesh mesh;
    std::vector<glm::vec3> rest;
    glm::mat4 view{1.f};
    glm::mat4 proj = glm::ortho(0.f, 2.f, 0.f, 2.f, -10.f, 10.f);
    PickFixture()
    {
        a = rig.addBone("A", {0, 0, 0}, {0, 1, 0}).id;
        b = rig.addChainBone(a, "B", {0, 2, 0}).id;
        std::vector<SkinVertex> verts;
        std::vector<uint32_t> idx;
        for (int q = 0; q < 4; ++q) {
            const float y0 = q * 0.5f, y1 = (q + 1) * 0.5f;
            const uint32_t base = static_cast<uint32_t>(verts.size());
            verts.push_back(vertexAt({0, y0, 0}));
            verts.push_back(vertexAt({1, y0, 0}));
            verts.push_back(vertexAt({1, y1, 0}));
            verts.push_back(vertexAt({0, y1, 0}));
            idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
        mesh = RS::makeRigMesh(verts, idx);
        for (const auto& v : mesh.vertices) rest.push_back(v.position);
    }
    RP::WeightView makeView(bool through = true) const { return RP::buildWeightView(mesh, rest, glm::mat4(1.f), view, proj, 200.f, 200.f, through); }
    int sourceAt(const glm::vec3& p) const
    {
        for (size_t i = 0; i < mesh.vertices.size(); ++i)
            if (glm::distance(mesh.vertices[i].position, p) < 1e-6f) return static_cast<int>(mesh.sourceIndex[i]);
        return -1;
    }
};

} // namespace

TEST(RigPickTest, ViewProjectsSourceVerticesToPixels)
{
    PickFixture f;
    const RP::WeightView v = f.makeView();
    ASSERT_EQ(v.pixel.size(), f.mesh.sourceCount);
    const int s = f.sourceAt({1, 0.5f, 0});
    ASSERT_GE(s, 0);
    EXPECT_NEAR(v.pixel[s].x, 100.f, 1e-3f);
    EXPECT_NEAR(v.pixel[s].y, 50.f, 1e-3f);
    EXPECT_TRUE(v.onScreen[s]);
    EXPECT_TRUE(v.visible[s]);
}

TEST(RigPickTest, NearestVisibleVertexWithinRange)
{
    PickFixture f;
    const RP::WeightView v = f.makeView();
    const int s = f.sourceAt({1, 1, 0});
    EXPECT_EQ(RP::nearestVisibleVertex(v, 102.f, 101.f), s);
    EXPECT_EQ(RP::nearestVisibleVertex(v, 150.f, 190.f, 5.f), -1);
}

TEST(RigPickTest, RectangleSelectsVisibleVertices)
{
    PickFixture f;
    const RP::WeightView v = f.makeView();
    // x in [90, 110] covers the x = 1 column (5 source vertices); the y range 40..110 keeps y = 0.5, 1.0.
    const auto picked = RP::visibleInRectangle(v, 110.f, 110.f, 90.f, 40.f);
    ASSERT_EQ(picked.size(), 2u);
    EXPECT_TRUE(std::find(picked.begin(), picked.end(), static_cast<uint32_t>(f.sourceAt({1, 0.5f, 0}))) != picked.end());
    EXPECT_TRUE(std::find(picked.begin(), picked.end(), static_cast<uint32_t>(f.sourceAt({1, 1, 0}))) != picked.end());
}

TEST(RigPickTest, OffScreenVerticesAreNeitherPickedNorVisible)
{
    PickFixture f;
    f.proj = glm::ortho(0.f, 0.5f, 0.f, 0.5f, -10.f, 10.f); // only the lower-left corner is in view
    const RP::WeightView v = f.makeView();
    const int far = f.sourceAt({1, 2, 0});
    ASSERT_GE(far, 0);
    EXPECT_FALSE(v.onScreen[far]);
    EXPECT_FALSE(v.visible[far]);
}

TEST(RigPickTest, DepthBufferHidesVerticesBehindASurface)
{
    // A front quad (z = 0) covering a vertex of a back quad (z = -1) hides it unless the pick goes through.
    SkinVertex v0 = vertexAt({0, 0, 0}), v1 = vertexAt({2, 0, 0}), v2 = vertexAt({2, 2, 0}), v3 = vertexAt({0, 2, 0});
    SkinVertex b0 = vertexAt({0.8f, 0.8f, -1}), b1 = vertexAt({1.2f, 0.8f, -1}), b2 = vertexAt({1.0f, 1.2f, -1});
    RigMesh mesh = RS::makeRigMesh({v0, v1, v2, v3, b0, b1, b2}, {0, 1, 2, 0, 2, 3, 4, 5, 6});
    std::vector<glm::vec3> rest;
    for (const auto& v : mesh.vertices) rest.push_back(v.position);
    // Perspective camera at z = 5 looking down -Z: nearer geometry has the smaller w.
    const glm::mat4 view = glm::lookAt(glm::vec3(1, 1, 5), glm::vec3(1, 1, 0), glm::vec3(0, 1, 0));
    const glm::mat4 proj = glm::perspective(glm::radians(40.f), 1.f, 0.1f, 50.f);
    const RP::WeightView hidden = RP::buildWeightView(mesh, rest, glm::mat4(1.f), view, proj, 200.f, 200.f, false);
    const RP::WeightView through = RP::buildWeightView(mesh, rest, glm::mat4(1.f), view, proj, 200.f, 200.f, true);
    EXPECT_EQ(hidden.visible[4], 0);
    EXPECT_EQ(through.visible[4], 1);
    EXPECT_EQ(hidden.visible[0], 1) << "the front quad's corner stays visible";
}

TEST(RigPickTest, BrushStrokeMovesWeightTowardTheBoneAtThePickedVertex)
{
    PickFixture f;
    RigBinding binding;
    ASSERT_TRUE(RS::rigidBind(f.rig, f.mesh, f.a, binding));
    const RP::WeightView v = f.makeView();
    const int s = f.sourceAt({1, 1, 0});
    int affected = 0;
    ASSERT_TRUE(RP::brushStroke(binding, f.rig, f.mesh, v, BrushMode::Add, f.b, 1.f, 0.3f, true, {glm::vec2(100.f, 100.f)}, affected));
    EXPECT_GT(affected, 0);
    float onB = 0.f;
    for (int k = 0; k < 4; ++k) if (binding.weights[s].boneId[k] == f.b) onB += binding.weights[s].weight[k];
    EXPECT_NEAR(onB, 1.f, 1e-4f);
    const int farSource = f.sourceAt({0, 0, 0});
    float far = 0.f;
    for (int k = 0; k < 4; ++k) if (binding.weights[farSource].boneId[k] == f.b) far += binding.weights[farSource].weight[k];
    EXPECT_EQ(far, 0.f) << "vertices beyond the radius are untouched";
}

TEST(RigPickTest, BrushStrokeSkipsPixelsWithoutAVertexAndRejectsAMismatchedView)
{
    PickFixture f;
    RigBinding binding;
    ASSERT_TRUE(RS::rigidBind(f.rig, f.mesh, f.a, binding));
    const RP::WeightView v = f.makeView();
    int affected = -1;
    ASSERT_TRUE(RP::brushStroke(binding, f.rig, f.mesh, v, BrushMode::Add, f.b, 1.f, 0.3f, true, {glm::vec2(190.f, 10.f)}, affected));
    EXPECT_EQ(affected, 0);
    RP::WeightView wrong = v;
    wrong.visible.pop_back();
    EXPECT_FALSE(RP::brushStroke(binding, f.rig, f.mesh, wrong, BrushMode::Add, f.b, 1.f, 0.3f, true, {glm::vec2(100.f, 100.f)}, affected));
}

TEST(RigPickTest, AutoBrushRadiusIsEightPercentOfTheDiagonal)
{
    PickFixture f;
    EXPECT_NEAR(RP::autoBrushRadius(f.mesh), 0.08f * std::sqrt(1.f + 4.f), 1e-5f);
}
