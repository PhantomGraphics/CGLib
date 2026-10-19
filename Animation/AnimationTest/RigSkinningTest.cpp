#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>
#include <limits>
#include <string>

#include "../Core/RigSkinning.h"

using namespace Phantom::Animation;
namespace RS = Phantom::Animation::RigSkinning;

namespace {

SkinVertex vertexAt(const glm::vec3& p)
{
    SkinVertex v;
    v.position = p;
    return v;
}

struct Fixture {
    AuthoredRig rig;
    uint32_t a = 0, b = 0;
    RigMesh mesh;
    Fixture()
    {
        a = rig.addBone("A", {0, 0, 0}, {0, 1, 0}).id;
        b = rig.addChainBone(a, "B", {0, 2, 0}).id;
        // Strip of quads along Y, vertices duplicated per quad (so render vertices > source vertices).
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
    }
};

float sum(const VertexInfluence& v) { return v.weight[0] + v.weight[1] + v.weight[2] + v.weight[3]; }

float weightOf(const VertexInfluence& v, uint32_t id)
{
    float w = 0.f;
    for (int k = 0; k < 4; ++k) if (v.boneId[k] == id) w += v.weight[k];
    return w;
}

} // namespace

TEST(RigSkinningTest, SourceIndicesWeldSharedPositions)
{
    Fixture f;
    EXPECT_EQ(f.mesh.vertices.size(), 16u);
    EXPECT_EQ(f.mesh.sourceCount, 10u); // 2 columns x 5 rows
    for (size_t i = 0; i < f.mesh.vertices.size(); ++i)
        for (size_t j = 0; j < f.mesh.vertices.size(); ++j)
            EXPECT_EQ(f.mesh.vertices[i].position == f.mesh.vertices[j].position,
                      f.mesh.sourceIndex[i] == f.mesh.sourceIndex[j]);
}

TEST(RigSkinningTest, AutoBindIsReproducibleNormalizedAndNearest)
{
    Fixture f;
    RigBinding b1, b2;
    ASSERT_TRUE(RS::autoBind(f.rig, f.mesh, b1).ok);
    ASSERT_TRUE(RS::autoBind(f.rig, f.mesh, b2).ok);
    ASSERT_EQ(b1.weights.size(), f.mesh.sourceCount);
    EXPECT_TRUE(RS::isBindingValid(b1, f.rig, f.mesh));
    const auto pos = RS::sourcePositions(f.mesh);
    for (size_t i = 0; i < b1.weights.size(); ++i) {
        EXPECT_EQ(std::memcmp(&b1.weights[i], &b2.weights[i], sizeof(VertexInfluence)), 0);
        EXPECT_NEAR(sum(b1.weights[i]), 1.f, 1e-5f);
        if (pos[i].y < 0.4f) EXPECT_GT(weightOf(b1.weights[i], f.a), weightOf(b1.weights[i], f.b));
        if (pos[i].y > 1.6f) EXPECT_GT(weightOf(b1.weights[i], f.b), weightOf(b1.weights[i], f.a));
    }
}

TEST(RigSkinningTest, AutoBindOnBoneIsFiniteAndTiesBreakById)
{
    AuthoredRig rig;
    const uint32_t a = rig.addBone("A", {0, 0, 0}, {0, 1, 0}).id;
    const uint32_t b = rig.addBone("B", {0, 0, 0}, {0, 1, 0}).id; // identical bone
    RigMesh mesh = RS::makeRigMesh({vertexAt({0, 0.5f, 0})}, {});
    RigBinding bind;
    ASSERT_TRUE(RS::autoBind(rig, mesh, bind).ok);
    const VertexInfluence& v = bind.weights[0];
    EXPECT_TRUE(std::isfinite(v.weight[0]) && std::isfinite(v.weight[1]));
    EXPECT_NEAR(sum(v), 1.f, 1e-5f);
    EXPECT_EQ(v.boneId[0], a); // equal weight -> lower id first
    EXPECT_EQ(v.boneId[1], b);
}

TEST(RigSkinningTest, AutoBindKeepsAtMostFourInfluences)
{
    AuthoredRig rig;
    for (int i = 0; i < 7; ++i) rig.addBone("b" + std::to_string(i), {float(i), 0, 0}, {float(i), 1, 0});
    RigMesh mesh = RS::makeRigMesh({vertexAt({3, 0.5f, 1})}, {});
    RigBinding bind;
    ASSERT_TRUE(RS::autoBind(rig, mesh, bind).ok);
    int used = 0;
    for (int k = 0; k < 4; ++k) used += bind.weights[0].boneId[k] != 0;
    EXPECT_EQ(used, 4);
    EXPECT_NEAR(sum(bind.weights[0]), 1.f, 1e-5f);
}

TEST(RigSkinningTest, AutoBindRejectsEmptyRig)
{
    Fixture f;
    AuthoredRig empty;
    RigBinding bind;
    EXPECT_FALSE(RS::autoBind(empty, f.mesh, bind).ok);
}

TEST(RigSkinningTest, RigidBind)
{
    Fixture f;
    RigBinding bind;
    ASSERT_TRUE(RS::rigidBind(f.rig, f.mesh, f.b, bind).ok);
    for (const auto& w : bind.weights) {
        EXPECT_EQ(w.boneId[0], f.b);
        EXPECT_FLOAT_EQ(w.weight[0], 1.f);
    }
    EXPECT_FALSE(RS::rigidBind(f.rig, f.mesh, 999, bind).ok);
}

TEST(RigSkinningTest, SetWeightsSanitizes)
{
    Fixture f;
    RigBinding bind;
    ASSERT_TRUE(RS::rigidBind(f.rig, f.mesh, f.a, bind).ok);
    ASSERT_TRUE(RS::setWeights(bind, f.rig, {0, 1}, {{f.a, 1.f}, {f.b, 3.f}, {f.a, 0.f}}).ok);
    EXPECT_NEAR(weightOf(bind.weights[0], f.b), 0.75f, 1e-5f);
    EXPECT_NEAR(sum(bind.weights[1]), 1.f, 1e-5f);
    // Unknown bones / out-of-range vertices are rejected without change.
    auto before = bind.weights;
    EXPECT_FALSE(RS::setWeights(bind, f.rig, {99}, {{f.a, 1.f}}).ok);
    EXPECT_FALSE(RS::setWeights(bind, f.rig, {0}, {{777, 1.f}}).ok);
    EXPECT_EQ(std::memcmp(before.data(), bind.weights.data(), sizeof(VertexInfluence) * before.size()), 0);
    // All-zero keeps the previous strongest bone.
    ASSERT_TRUE(RS::setWeights(bind, f.rig, {0}, {{f.a, 0.f}, {f.b, -1.f}}).ok);
    EXPECT_EQ(bind.weights[0].boneId[0], f.b);
    EXPECT_FLOAT_EQ(bind.weights[0].weight[0], 1.f);
}

TEST(RigSkinningTest, BrushAddSubtractKeepNormalization)
{
    Fixture f;
    RigBinding bind;
    ASSERT_TRUE(RS::rigidBind(f.rig, f.mesh, f.a, bind).ok);
    const auto adj = RS::buildAdjacency(f.mesh);
    ASSERT_TRUE(RS::applyBrush(bind, f.rig, adj, BrushMode::Add, f.b, 0.4f, {{3, 1.f}}).ok);
    EXPECT_NEAR(weightOf(bind.weights[3], f.b), 0.4f, 1e-5f);
    EXPECT_NEAR(weightOf(bind.weights[3], f.a), 0.6f, 1e-5f);
    ASSERT_TRUE(RS::applyBrush(bind, f.rig, adj, BrushMode::Subtract, f.b, 1.f, {{3, 0.5f}}).ok);
    EXPECT_NEAR(weightOf(bind.weights[3], f.b), 0.0f, 1e-5f);
    EXPECT_NEAR(sum(bind.weights[3]), 1.f, 1e-5f);
    // Strength 1 Add saturates at 1 and the others drop out.
    ASSERT_TRUE(RS::applyBrush(bind, f.rig, adj, BrushMode::Add, f.b, 1.f, {{4, 1.f}}).ok);
    EXPECT_NEAR(weightOf(bind.weights[4], f.b), 1.f, 1e-5f);
    // Subtracting the only influence falls back to the previous strongest bone, never an all-zero vertex.
    ASSERT_TRUE(RS::applyBrush(bind, f.rig, adj, BrushMode::Subtract, f.b, 1.f, {{4, 1.f}}).ok);
    EXPECT_NEAR(sum(bind.weights[4]), 1.f, 1e-5f);
    EXPECT_TRUE(RS::isBindingValid(bind, f.rig, f.mesh));
}

TEST(RigSkinningTest, SmoothBlendsTowardNeighboursIndependentOfOrder)
{
    Fixture f;
    RigBinding bind;
    ASSERT_TRUE(RS::rigidBind(f.rig, f.mesh, f.a, bind).ok);
    const auto pos = RS::sourcePositions(f.mesh);
    for (uint32_t v = 0; v < f.mesh.sourceCount; ++v)
        if (pos[v].y > 0.9f) ASSERT_TRUE(RS::setWeights(bind, f.rig, {v}, {{f.b, 1.f}}).ok);
    const auto adj = RS::buildAdjacency(f.mesh);
    std::vector<BrushSample> all, rev;
    for (uint32_t v = 0; v < f.mesh.sourceCount; ++v) all.push_back({v, 1.f});
    rev.assign(all.rbegin(), all.rend());
    RigBinding b2 = bind;
    ASSERT_TRUE(RS::applyBrush(bind, f.rig, adj, BrushMode::Smooth, 0, 0.5f, all).ok);
    ASSERT_TRUE(RS::applyBrush(b2, f.rig, adj, BrushMode::Smooth, 0, 0.5f, rev).ok);
    bool changed = false;
    for (size_t i = 0; i < bind.weights.size(); ++i) {
        EXPECT_EQ(std::memcmp(&bind.weights[i], &b2.weights[i], sizeof(VertexInfluence)), 0);
        EXPECT_NEAR(sum(bind.weights[i]), 1.f, 1e-5f);
        if (pos[i].y < 0.9f && weightOf(bind.weights[i], f.b) > 0.f) changed = true;
    }
    EXPECT_TRUE(changed); // weight bled across the joint
}

TEST(RigSkinningTest, SplitRenderVerticesShareWeights)
{
    Fixture f;
    RigBinding bind;
    ASSERT_TRUE(RS::autoBind(f.rig, f.mesh, bind).ok);
    ASSERT_TRUE(RS::applyBrush(bind, f.rig, RS::buildAdjacency(f.mesh), BrushMode::Add, f.b, 0.3f, {{2, 1.f}, {5, 0.5f}}).ok);
    Skeleton sk;
    SkinnedMesh sm;
    ASSERT_TRUE(RS::buildSkinnedMesh(f.rig, bind, f.mesh, sk, sm).ok);
    for (size_t i = 0; i < sm.vertices.size(); ++i)
        for (size_t j = 0; j < sm.vertices.size(); ++j)
            if (sm.vertices[i].position == sm.vertices[j].position) {
                EXPECT_EQ(sm.vertices[i].boneIndices, sm.vertices[j].boneIndices);
                EXPECT_EQ(sm.vertices[i].boneWeights, sm.vertices[j].boneWeights);
            }
}

TEST(RigSkinningTest, SelectInRadiusAndVisibility)
{
    Fixture f;
    const auto pos = RS::sourcePositions(f.mesh);
    auto hits = RS::selectInRadius(pos, {0, 0, 0}, 0.6f);
    ASSERT_FALSE(hits.empty());
    bool centre = false;
    for (const auto& h : hits) {
        EXPECT_GT(h.falloff, 0.f);
        EXPECT_LE(h.falloff, 1.f);
        EXPECT_LT(glm::distance(pos[h.sourceVertex], glm::vec3(0)), 0.6f);
        if (pos[h.sourceVertex] == glm::vec3(0)) { centre = true; EXPECT_FLOAT_EQ(h.falloff, 1.f); }
    }
    EXPECT_TRUE(centre);
    std::vector<uint8_t> hidden(pos.size(), 0);
    EXPECT_TRUE(RS::selectInRadius(pos, {0, 0, 0}, 0.6f, &hidden).empty());
    EXPECT_TRUE(RS::selectInRadius(pos, {0, 0, 0}, 0.f).empty());
}

TEST(RigSkinningTest, BindPoseSkinningReproducesRigSpaceVertices)
{
    Fixture f;
    RigBinding bind;
    ASSERT_TRUE(RS::autoBind(f.rig, f.mesh, bind).ok);
    Skeleton sk;
    SkinnedMesh sm;
    ASSERT_TRUE(RS::buildSkinnedMesh(f.rig, bind, f.mesh, sk, sm).ok);
    std::vector<glm::mat4> world(sk.bones.size());
    for (size_t i = 0; i < sk.bones.size(); ++i) {
        const Bone& bn = sk.bones[i];
        glm::mat4 local = glm::translate(glm::mat4(1.f), bn.localPosition) * glm::mat4_cast(bn.localRotation);
        world[i] = bn.parentIndex < 0 ? local : world[bn.parentIndex] * local;
    }
    for (const auto& v : sm.vertices) {
        glm::vec3 out(0.f);
        for (int k = 0; k < 4; ++k) {
            if (v.boneWeights[k] <= 0.f) continue;
            const int bi = v.boneIndices[k];
            out += v.boneWeights[k] * glm::vec3(world[bi] * sk.bones[bi].bindPoseInverse * glm::vec4(v.position, 1.f));
        }
        EXPECT_NEAR(glm::distance(out, v.position), 0.f, 1e-5f);
    }
}

TEST(RigSkinningTest, StaleBindingIsRejected)
{
    Fixture f;
    RigBinding bind;
    ASSERT_TRUE(RS::autoBind(f.rig, f.mesh, bind).ok);
    ASSERT_TRUE(f.rig.renameBone(f.a, "Renamed").ok); // any structural edit bumps the revision
    EXPECT_FALSE(RS::isBindingValid(bind, f.rig, f.mesh));
    Skeleton sk;
    SkinnedMesh sm;
    EXPECT_FALSE(RS::buildSkinnedMesh(f.rig, bind, f.mesh, sk, sm).ok);
}

TEST(RigSkinningTest, TransformToRigSpaceHandlesNonUniformAndNegativeScale)
{
    Fixture f;
    RigMesh m = f.mesh;
    const glm::mat4 xf = glm::scale(glm::translate(glm::mat4(1.f), glm::vec3(2, 0, 0)), glm::vec3(2.f, 1.f, 0.5f));
    ASSERT_TRUE(RS::transformToRigSpace(m, xf).ok);
    EXPECT_EQ(m.vertices[1].position, glm::vec3(4, 0, 0));
    EXPECT_NEAR(glm::length(m.vertices[0].normal), 1.f, 1e-5f);

    RigMesh mirrored = f.mesh;
    ASSERT_TRUE(RS::transformToRigSpace(mirrored, glm::scale(glm::mat4(1.f), glm::vec3(-1.f, 1.f, 1.f))).ok);
    // Winding is flipped so a mirrored front face stays front facing (+Z for these XY quads).
    const auto triZ = [](const RigMesh& r) {
        const auto& p0 = r.vertices[r.indices[0]].position;
        const auto& p1 = r.vertices[r.indices[1]].position;
        const auto& p2 = r.vertices[r.indices[2]].position;
        return glm::cross(p1 - p0, p2 - p0).z;
    };
    EXPECT_GT(triZ(f.mesh), 0.f);
    EXPECT_GT(triZ(mirrored), 0.f);
}

TEST(RigSkinningTest, TransformRejectsSingularAndNonFinite)
{
    Fixture f;
    RigMesh m = f.mesh;
    EXPECT_FALSE(RS::transformToRigSpace(m, glm::scale(glm::mat4(1.f), glm::vec3(1.f, 0.f, 1.f))).ok);
    glm::mat4 bad(1.f);
    bad[3][0] = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(RS::transformToRigSpace(m, bad).ok);
    EXPECT_EQ(m.vertices[1].position, f.mesh.vertices[1].position);
}
