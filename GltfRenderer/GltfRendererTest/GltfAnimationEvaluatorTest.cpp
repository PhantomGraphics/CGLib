#include "gtest/gtest.h"

#include "../Gltf/GltfAccessorBuilder.h"
#include "../Gltf/GltfAnimationEvaluator.h"

using namespace Phantom::Gltf;

namespace {

// One root node (translation-animated), skinned by a trivial identity-inverse-bind-matrix skin,
// plus a second node holding a 2-target Weights-animated mesh (targets/weights themselves are
// irrelevant to the evaluator -- it only reads channel/sampler data).
GltfDocument makeDoc()
{
    GltfDocument doc;

    GltfNode node;
    node.name = "Root";
    doc.nodes.push_back(node); // index 0

    GltfNode meshNode;
    meshNode.name = "Mesh";
    doc.nodes.push_back(meshNode); // index 1

    GltfScene scene;
    scene.nodes = {0, 1};
    doc.scenes.push_back(scene);
    doc.defaultScene = 0;

    GltfSkin skin;
    skin.joints = {0};
    skin.inverseBindMatrices = {glm::mat4(1.f)};
    doc.skins.push_back(skin);

    GltfAnimation anim;

    // Translation channel on node 0: (0,0,0) at t=0 -> (2,0,0) at t=1.
    {
        std::vector<float> times = {0.f, 1.f};
        std::vector<glm::vec3> values = {glm::vec3(0.f), glm::vec3(2.f, 0.f, 0.f)};
        GltfAnimationSampler sampler;
        sampler.input  = appendAccessor(doc, times, GltfComponentType::Float, GltfAccessorType::Scalar);
        sampler.output = appendAccessor(doc, values, GltfComponentType::Float, GltfAccessorType::Vec3);
        const int samplerIdx = static_cast<int>(anim.samplers.size());
        anim.samplers.push_back(sampler);
        anim.channels.push_back({samplerIdx, {0, GltfAnimationPath::Translation}});
    }

    // Rotation channel on node 0: identity at t=0 -> 180deg about Y (quat (0,1,0,0)) at t=2.
    {
        std::vector<float> times = {0.f, 2.f};
        std::vector<glm::vec4> values = {glm::vec4(0.f, 0.f, 0.f, 1.f), glm::vec4(0.f, 1.f, 0.f, 0.f)};
        GltfAnimationSampler sampler;
        sampler.input  = appendAccessor(doc, times, GltfComponentType::Float, GltfAccessorType::Scalar);
        sampler.output = appendAccessor(doc, values, GltfComponentType::Float, GltfAccessorType::Vec4);
        const int samplerIdx = static_cast<int>(anim.samplers.size());
        anim.samplers.push_back(sampler);
        anim.channels.push_back({samplerIdx, {0, GltfAnimationPath::Rotation}});
    }

    // Weights channel on node 1: [0,0] at t=0 -> [1,0.5] at t=1 (2 targets).
    {
        std::vector<float> times = {0.f, 1.f};
        std::vector<float> flatWeights = {0.f, 0.f, 1.f, 0.5f};
        GltfAnimationSampler sampler;
        sampler.input  = appendAccessor(doc, times, GltfComponentType::Float, GltfAccessorType::Scalar);
        sampler.output = appendAccessor(doc, flatWeights, GltfComponentType::Float, GltfAccessorType::Scalar);
        const int samplerIdx = static_cast<int>(anim.samplers.size());
        anim.samplers.push_back(sampler);
        anim.channels.push_back({samplerIdx, {1, GltfAnimationPath::Weights}});
    }

    doc.animations.push_back(anim);
    return doc;
}

} // namespace

// -----------------------------------------------------------------------
// evaluateSkin
// -----------------------------------------------------------------------

TEST(GltfAnimationEvaluatorTest, TranslationAtKeyframeBoundaries)
{
    const GltfDocument doc = makeDoc();

    auto matAt0 = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, 0.f);
    ASSERT_EQ(1u, matAt0.size());
    EXPECT_FLOAT_EQ(0.f, matAt0[0][3][0]);

    auto matAt1 = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, 1.f);
    ASSERT_EQ(1u, matAt1.size());
    EXPECT_FLOAT_EQ(2.f, matAt1[0][3][0]);
}

TEST(GltfAnimationEvaluatorTest, TranslationLinearlyInterpolatedMidway)
{
    const GltfDocument doc = makeDoc();
    auto mat = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, 0.5f);
    ASSERT_EQ(1u, mat.size());
    EXPECT_NEAR(1.f, mat[0][3][0], 1e-5f);
}

TEST(GltfAnimationEvaluatorTest, TranslationClampsOutOfRangeTimes)
{
    const GltfDocument doc = makeDoc();

    auto before = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, -5.f);
    EXPECT_NEAR(0.f, before[0][3][0], 1e-5f);

    auto after = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, 100.f);
    EXPECT_NEAR(2.f, after[0][3][0], 1e-5f);
}

TEST(GltfAnimationEvaluatorTest, RotationSlerpsMidway)
{
    const GltfDocument doc = makeDoc();
    // At t=1 (midway through the 0..2s rotation channel), expect a 90deg rotation about Y:
    // (0,1,0) rotates to (0,0,-1) under a +90deg-about-Y rotation.
    auto mat = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, 1.f);
    const glm::vec4 rotated = mat[0] * glm::vec4(1.f, 0.f, 0.f, 0.f);
    EXPECT_NEAR(0.f,  rotated.x, 1e-4f);
    EXPECT_NEAR(0.f,  rotated.y, 1e-4f);
    EXPECT_NEAR(-1.f, rotated.z, 1e-4f);
}

TEST(GltfAnimationEvaluatorTest, InvalidAnimationIndexFallsBackToStaticPose)
{
    const GltfDocument doc = makeDoc();
    auto mat = GltfAnimationEvaluator::evaluateSkin(doc, -1, 0, 0.5f);
    ASSERT_EQ(1u, mat.size());
    EXPECT_EQ(glm::mat4(1.f), mat[0]); // node 0's default TRS is identity
}

TEST(GltfAnimationEvaluatorTest, InvalidSkinIndexReturnsEmpty)
{
    const GltfDocument doc = makeDoc();
    auto mat = GltfAnimationEvaluator::evaluateSkin(doc, 0, 99, 0.5f);
    EXPECT_TRUE(mat.empty());
}

// -----------------------------------------------------------------------
// evaluateMorphWeights
// -----------------------------------------------------------------------

TEST(GltfAnimationEvaluatorTest, MorphWeightsAtKeyframeBoundaries)
{
    const GltfDocument doc = makeDoc();

    auto w0 = GltfAnimationEvaluator::evaluateMorphWeights(doc, 0, 1, 2, 0.f);
    ASSERT_EQ(2u, w0.size());
    EXPECT_FLOAT_EQ(0.f, w0[0]);
    EXPECT_FLOAT_EQ(0.f, w0[1]);

    auto w1 = GltfAnimationEvaluator::evaluateMorphWeights(doc, 0, 1, 2, 1.f);
    ASSERT_EQ(2u, w1.size());
    EXPECT_FLOAT_EQ(1.0f, w1[0]);
    EXPECT_FLOAT_EQ(0.5f, w1[1]);
}

TEST(GltfAnimationEvaluatorTest, MorphWeightsInterpolatedMidway)
{
    const GltfDocument doc = makeDoc();
    auto w = GltfAnimationEvaluator::evaluateMorphWeights(doc, 0, 1, 2, 0.5f);
    ASSERT_EQ(2u, w.size());
    EXPECT_NEAR(0.5f,  w[0], 1e-5f);
    EXPECT_NEAR(0.25f, w[1], 1e-5f);
}

TEST(GltfAnimationEvaluatorTest, MorphWeightsNoMatchingChannelReturnsZeros)
{
    const GltfDocument doc = makeDoc();
    // Node 0 has no Weights channel.
    auto w = GltfAnimationEvaluator::evaluateMorphWeights(doc, 0, 0, 3, 0.5f);
    ASSERT_EQ(3u, w.size());
    for (float v : w) EXPECT_FLOAT_EQ(0.f, v);
}

// -----------------------------------------------------------------------
// duration
// -----------------------------------------------------------------------

TEST(GltfAnimationEvaluatorTest, DurationIsLatestKeyframeAcrossSamplers)
{
    const GltfDocument doc = makeDoc();
    // Translation ends at t=1, rotation at t=2, weights at t=1 -> max is 2.
    EXPECT_FLOAT_EQ(2.f, GltfAnimationEvaluator::duration(doc.animations[0], doc));
}
