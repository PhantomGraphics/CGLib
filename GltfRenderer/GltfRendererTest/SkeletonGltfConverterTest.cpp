#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../Gltf/GltfAccessorView.h"
#include "../Gltf/SkeletonGltfConverter.h"

using namespace Phantom::Gltf;
using namespace Phantom::Animation;

namespace {

// 2-bone chain, 2 vertices (one bound to each bone), no materials/textures.
void makeSkeletonAndMesh(Skeleton& sk, SkinnedMesh& mesh)
{
    Bone root;
    root.name         = "Root";
    root.parentIndex  = -1;
    root.localPosition = {0.f, 0.f, 0.f};
    sk.addBone(root);

    Bone child;
    child.name         = "Child";
    child.parentIndex  = 0;
    child.localPosition = {0.f, 1.f, 0.f};
    sk.addBone(child);

    SkinVertex v0; v0.position = {0.f, 0.f, 0.f}; v0.boneIndices = {0,0,0,0}; v0.boneWeights = {1,0,0,0};
    SkinVertex v1; v1.position = {0.f, 1.f, 0.f}; v1.boneIndices = {1,0,0,0}; v1.boneWeights = {1,0,0,0};
    mesh.vertices = {v0, v1};
    mesh.indices  = {0, 1, 0}; // degenerate triangle, index data isn't the focus of this test
}

} // namespace

TEST(SkeletonGltfConverterTest, StaticOverload_NoAnimationsNoMorphs)
{
    Skeleton sk; SkinnedMesh mesh;
    makeSkeletonAndMesh(sk, mesh);

    GltfDocument doc;
    SkeletonGltfConverter conv;
    ASSERT_TRUE(conv.convert(sk, mesh, {}, {}, "", doc));

    EXPECT_TRUE(doc.animations.empty());
    ASSERT_EQ(1u, doc.meshes.size());
    ASSERT_EQ(1u, doc.meshes[0].primitives.size());
    EXPECT_TRUE(doc.meshes[0].primitives[0].targets.empty());
    EXPECT_TRUE(doc.meshes[0].weights.empty());
}

TEST(SkeletonGltfConverterTest, BakedClip_BuildsBoneAnimationChannels)
{
    Skeleton sk; SkinnedMesh mesh;
    makeSkeletonAndMesh(sk, mesh);

    AnimationClip clip;
    clip.duration = 1.f;
    BoneChannel ch;
    ch.boneIndex = 1; // Child
    ch.positionKeys.push_back({0.f, glm::vec3{0.f, 0.f, 0.f}});
    ch.positionKeys.push_back({1.f, glm::vec3{0.f, 0.5f, 0.f}});
    ch.rotationKeys.push_back({0.f, glm::quat{1.f, 0.f, 0.f, 0.f}});
    ch.rotationKeys.push_back({1.f, glm::angleAxis(glm::radians(90.f), glm::vec3{0.f, 0.f, 1.f})});
    clip.channels.push_back(ch);

    GltfDocument doc;
    SkeletonGltfConverter conv;
    ASSERT_TRUE(conv.convert(sk, mesh, {}, {}, "", clip, {}, {}, doc));

    ASSERT_EQ(1u, doc.animations.size());
    const GltfAnimation& anim = doc.animations[0];
    ASSERT_EQ(2u, anim.channels.size()); // Translation + Rotation (no scale keys supplied)

    bool sawTranslation = false, sawRotation = false;
    for (const auto& gch : anim.channels) {
        EXPECT_EQ(1, gch.target.node); // bone index 1 -> node index 1 (1:1 mapping)
        const GltfAnimationSampler& sampler = anim.samplers[gch.samplerIndex];
        GltfAccessorView times(doc, sampler.input);
        ASSERT_EQ(2u, times.count());
        EXPECT_FLOAT_EQ(0.f, times.get<float>(0));
        EXPECT_FLOAT_EQ(1.f, times.get<float>(1));

        if (gch.target.path == GltfAnimationPath::Translation) {
            sawTranslation = true;
            GltfAccessorView values(doc, sampler.output);
            // Output = bone.localPosition + key.value (delta convention).
            const glm::vec3 v0 = values.get<glm::vec3>(0);
            const glm::vec3 v1 = values.get<glm::vec3>(1);
            EXPECT_NEAR(1.f, v0.y, 1e-5f); // localPosition.y(1) + key(0)
            EXPECT_NEAR(1.5f, v1.y, 1e-5f); // localPosition.y(1) + key(0.5)
        } else if (gch.target.path == GltfAnimationPath::Rotation) {
            sawRotation = true;
            GltfAccessorView values(doc, sampler.output);
            const glm::vec4 q0 = values.get<glm::vec4>(0);
            EXPECT_NEAR(0.f, q0.x, 1e-5f);
            EXPECT_NEAR(0.f, q0.y, 1e-5f);
            EXPECT_NEAR(0.f, q0.z, 1e-5f);
            EXPECT_NEAR(1.f, q0.w, 1e-5f);
        }
    }
    EXPECT_TRUE(sawTranslation);
    EXPECT_TRUE(sawRotation);
}

TEST(SkeletonGltfConverterTest, MorphTargetsAndBakedWeights_BuildMorphDataAndWeightsChannel)
{
    Skeleton sk; SkinnedMesh mesh;
    makeSkeletonAndMesh(sk, mesh);

    MorphTarget target;
    target.name = "Blink";
    target.deltas.push_back({0, glm::vec3{0.1f, 0.f, 0.f}}); // only vertex 0 moves

    std::vector<std::pair<float, std::vector<float>>> bakedWeights = {
        {0.f, {0.f}},
        {1.f, {1.f}},
    };

    GltfDocument doc;
    SkeletonGltfConverter conv;
    ASSERT_TRUE(conv.convert(sk, mesh, {}, {}, "", AnimationClip{}, {target}, bakedWeights, doc));

    ASSERT_EQ(1u, doc.meshes.size());
    ASSERT_EQ(1u, doc.meshes[0].primitives.size());
    const GltfPrimitive& prim = doc.meshes[0].primitives[0];
    ASSERT_EQ(1u, prim.targets.size());

    GltfAccessorView deltaView(doc, prim.targets[0].positionAccessor);
    ASSERT_EQ(2u, deltaView.count()); // one entry per base vertex
    const glm::vec3 d0 = deltaView.get<glm::vec3>(0);
    const glm::vec3 d1 = deltaView.get<glm::vec3>(1);
    EXPECT_NEAR(0.1f, d0.x, 1e-5f);
    EXPECT_NEAR(0.f,  d1.x, 1e-5f); // untouched vertex defaults to zero offset

    ASSERT_EQ(1u, doc.meshes[0].weights.size());
    EXPECT_FLOAT_EQ(0.f, doc.meshes[0].weights[0]);

    // The mesh-holding node ("SkinnedMeshRoot") mirrors the mesh's default weights and is the
    // Weights channel's target.
    const int meshNodeIndex = static_cast<int>(doc.nodes.size()) - 1;
    ASSERT_EQ(1u, doc.nodes[meshNodeIndex].weights.size());

    ASSERT_EQ(1u, doc.animations.size());
    const GltfAnimation& anim = doc.animations[0];
    ASSERT_EQ(1u, anim.channels.size());
    EXPECT_EQ(GltfAnimationPath::Weights, anim.channels[0].target.path);
    EXPECT_EQ(meshNodeIndex, anim.channels[0].target.node);

    const GltfAnimationSampler& sampler = anim.samplers[anim.channels[0].samplerIndex];
    GltfAccessorView outView(doc, sampler.output);
    ASSERT_EQ(2u, outView.count()); // 2 keyframes x 1 target
    EXPECT_FLOAT_EQ(0.f, outView.get<float>(0));
    EXPECT_FLOAT_EQ(1.f, outView.get<float>(1));
}
