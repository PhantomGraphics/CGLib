#include "gtest/gtest.h"

#include "../Gltf/GltfAccessorBuilder.h"
#include "../Gltf/GltfMorphApply.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/geometric.hpp>

using namespace Phantom::Gltf;

namespace {

// One primitive, 2 vertices, 2 morph targets. Target 0 ("Inflate") pushes both verts out +X and
// tilts the normal; target 1 ("Twist") pushes vert 1 along +Z with no normal delta.
GltfDocument makeMorphDoc()
{
    GltfDocument doc;

    std::vector<glm::vec3> basePos = {{0, 0, 0}, {0, 1, 0}};
    std::vector<glm::vec3> baseNrm = {{0, 0, 1}, {0, 0, 1}};
    GltfPrimitive prim;
    prim.positionAccessor = appendAccessor(doc, basePos, GltfComponentType::Float, GltfAccessorType::Vec3);
    prim.normalAccessor   = appendAccessor(doc, baseNrm, GltfComponentType::Float, GltfAccessorType::Vec3);

    std::vector<glm::vec3> t0pos = {{1, 0, 0}, {1, 0, 0}};
    std::vector<glm::vec3> t0nrm = {{1, 0, 0}, {1, 0, 0}}; // rotates the normal toward +X
    GltfMorphTarget t0;
    t0.positionAccessor = appendAccessor(doc, t0pos, GltfComponentType::Float, GltfAccessorType::Vec3);
    t0.normalAccessor   = appendAccessor(doc, t0nrm, GltfComponentType::Float, GltfAccessorType::Vec3);
    prim.targets.push_back(t0);

    std::vector<glm::vec3> t1pos = {{0, 0, 0}, {0, 0, 2}};
    GltfMorphTarget t1;
    t1.positionAccessor = appendAccessor(doc, t1pos, GltfComponentType::Float, GltfAccessorType::Vec3);
    prim.targets.push_back(t1); // no normalAccessor

    GltfMesh mesh; mesh.primitives.push_back(prim);
    doc.meshes.push_back(mesh);
    return doc;
}

} // namespace

TEST(GltfMorphApplyTest, PositionsBlendAdditivelyFromBase)
{
    const GltfDocument doc = makeMorphDoc();
    const GltfPrimitive& prim = doc.meshes[0].primitives[0];

    auto rest = applyMorphs(doc, prim, {0.f, 0.f});
    EXPECT_NEAR(0.f, glm::length(rest[0] - glm::vec3(0, 0, 0)), 1e-5f);
    EXPECT_NEAR(0.f, glm::length(rest[1] - glm::vec3(0, 1, 0)), 1e-5f);

    // Half Inflate + full Twist: relative deltas sum (not sequential).
    auto both = applyMorphs(doc, prim, {0.5f, 1.f});
    EXPECT_NEAR(0.f, glm::length(both[0] - glm::vec3(0.5f, 0, 0)), 1e-5f);
    EXPECT_NEAR(0.f, glm::length(both[1] - glm::vec3(0.5f, 1, 2)), 1e-5f);
}

TEST(GltfMorphApplyTest, ReturningToZeroWeightsRestoresExactBase)
{
    const GltfDocument doc = makeMorphDoc();
    const GltfPrimitive& prim = doc.meshes[0].primitives[0];
    applyMorphs(doc, prim, {1.f, 1.f});          // deform
    auto back = applyMorphs(doc, prim, {0.f, 0.f}); // and back -- must be pristine base, no drift
    EXPECT_NEAR(0.f, glm::length(back[0] - glm::vec3(0, 0, 0)), 1e-6f);
    EXPECT_NEAR(0.f, glm::length(back[1] - glm::vec3(0, 1, 0)), 1e-6f);
}

TEST(GltfMorphApplyTest, NormalsBlendAndRenormalize)
{
    const GltfDocument doc = makeMorphDoc();
    const GltfPrimitive& prim = doc.meshes[0].primitives[0];

    auto rest = applyMorphedNormals(doc, prim, {0.f, 0.f});
    EXPECT_NEAR(0.f, glm::length(rest[0] - glm::vec3(0, 0, 1)), 1e-5f);

    // Full Inflate: base (0,0,1) + delta (1,0,0) = (1,0,1) -> normalized.
    auto inflated = applyMorphedNormals(doc, prim, {1.f, 0.f});
    EXPECT_NEAR(1.f, glm::length(inflated[0]), 1e-5f);
    EXPECT_NEAR(0.70710678f, inflated[0].x, 1e-4f);
    EXPECT_NEAR(0.70710678f, inflated[0].z, 1e-4f);

    // Twist has no normal delta -> weighting it changes nothing.
    auto twisted = applyMorphedNormals(doc, prim, {0.f, 1.f});
    EXPECT_NEAR(0.f, glm::length(twisted[1] - glm::vec3(0, 0, 1)), 1e-5f);
}

TEST(GltfMorphApplyTest, NoAccessorReturnsEmpty)
{
    GltfDocument doc;
    GltfPrimitive prim; // no POSITION / NORMAL
    EXPECT_TRUE(applyMorphs(doc, prim, {1.f}).empty());
    EXPECT_TRUE(applyMorphedNormals(doc, prim, {1.f}).empty());
}
