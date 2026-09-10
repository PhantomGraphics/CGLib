#include "gtest/gtest.h"

#include "../Gltf/GltfAccessorBuilder.h"
#include "../Gltf/GltfAnimationEvaluator.h"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>

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

// -----------------------------------------------------------------------
// evaluateNodeGlobalTransforms  (object animation -- Blender->Universe Phase 3)
// -----------------------------------------------------------------------

namespace {

// Parent node 0 (rotation-animated) with child node 1 (mesh) offset by +1 on X. Used to check
// that a child inherits its animated ancestor's transform, and STEP/CUBICSPLINE interpolation.
GltfDocument makeObjectAnimDoc(GltfInterpolation interp)
{
    GltfDocument doc;

    GltfNode parent; parent.name = "Pivot"; parent.children = {1};
    doc.nodes.push_back(parent);
    GltfNode child;  child.name = "Blade"; child.translation = glm::vec3(1.f, 0.f, 0.f); child.meshIndex = 0;
    doc.nodes.push_back(child);

    GltfScene scene; scene.nodes = {0}; doc.scenes.push_back(scene);
    doc.defaultScene = 0;

    GltfMesh mesh; doc.meshes.push_back(mesh);

    GltfAnimation anim;
    GltfAnimationSampler s;
    s.interpolation = interp;
    std::vector<float> times = {0.f, 1.f, 2.f};
    s.input = appendAccessor(doc, times, GltfComponentType::Float, GltfAccessorType::Scalar);

    // Translation on the pivot: 0 -> 10 -> 20 on X.
    if (interp == GltfInterpolation::CubicSpline) {
        // [inTangent, value, outTangent] per keyframe. Zero tangents = flat Hermite segments.
        std::vector<glm::vec3> v = {
            {0,0,0}, {0,0,0},  {0,0,0},   // key 0: value 0
            {0,0,0}, {10,0,0}, {0,0,0},   // key 1: value 10
            {0,0,0}, {20,0,0}, {0,0,0},   // key 2: value 20
        };
        s.output = appendAccessor(doc, v, GltfComponentType::Float, GltfAccessorType::Vec3);
    } else {
        std::vector<glm::vec3> v = {{0,0,0}, {10,0,0}, {20,0,0}};
        s.output = appendAccessor(doc, v, GltfComponentType::Float, GltfAccessorType::Vec3);
    }
    anim.samplers.push_back(s);
    anim.channels.push_back({0, {0, GltfAnimationPath::Translation}});
    doc.animations.push_back(anim);
    return doc;
}

} // namespace

TEST(GltfAnimationEvaluatorTest, NodeGlobalTransformsPropagateToChildren)
{
    const GltfDocument doc = makeObjectAnimDoc(GltfInterpolation::Linear);
    // t=0.5 -> pivot translated (5,0,0); child sits at local (1,0,0) -> global (6,0,0).
    auto g = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 0.5f);
    ASSERT_EQ(2u, g.size());
    EXPECT_NEAR(5.f, g[0][3][0], 1e-4f);
    EXPECT_NEAR(6.f, g[1][3][0], 1e-4f);
}

TEST(GltfAnimationEvaluatorTest, StepInterpolationHoldsLeftKeyframe)
{
    GltfDocument doc = makeObjectAnimDoc(GltfInterpolation::Step);
    // STEP: any time in [0,1) holds key 0's value (0), not a blend toward 10.
    auto mid = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 0.99f);
    EXPECT_NEAR(0.f, mid[0][3][0], 1e-4f);
    auto onKey = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 1.f);
    EXPECT_NEAR(10.f, onKey[0][3][0], 1e-4f);
}

TEST(GltfAnimationEvaluatorTest, CubicSplineInterpolatesWithHermiteBasis)
{
    GltfDocument doc = makeObjectAnimDoc(GltfInterpolation::CubicSpline);
    // Zero tangents -> Hermite reduces to smoothstep: at s=0.5, value = 0.5*(v0+v1) only when
    // symmetric; the 2s^3-3s^2 basis gives exactly the midpoint for equal endpoints spacing.
    auto mid = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 0.5f);
    EXPECT_NEAR(5.f, mid[0][3][0], 1e-4f); // (2*.125 - 3*.25 + 1)*0 + (-2*.125 + 3*.25)*10 = 5
    auto key = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 2.f);
    EXPECT_NEAR(20.f, key[0][3][0], 1e-4f);
}

// -----------------------------------------------------------------------
// Nested chains with several animated nodes at different depths, plus a
// Scale channel -- mirrors testdata/blender_input_crane (Slew_Pivot rotates,
// Trolley slides along it, Payload_Lift drops from the trolley, Cable stretches).
// -----------------------------------------------------------------------

namespace {

int appendScalarTimes(GltfDocument& doc, const std::vector<float>& times)
{
    return appendAccessor(doc, times, GltfComponentType::Float, GltfAccessorType::Scalar);
}

int appendVec3Sampler(GltfDocument& doc, GltfAnimation& anim, int timesAcc,
                       const std::vector<glm::vec3>& values)
{
    GltfAnimationSampler s;
    s.interpolation = GltfInterpolation::Linear;
    s.input  = timesAcc;
    s.output = appendAccessor(doc, values, GltfComponentType::Float, GltfAccessorType::Vec3);
    const int idx = static_cast<int>(anim.samplers.size());
    anim.samplers.push_back(s);
    return idx;
}

int appendQuatSampler(GltfDocument& doc, GltfAnimation& anim, int timesAcc,
                       const std::vector<glm::vec4>& values)
{
    GltfAnimationSampler s;
    s.interpolation = GltfInterpolation::Linear;
    s.input  = timesAcc;
    s.output = appendAccessor(doc, values, GltfComponentType::Float, GltfAccessorType::Vec4);
    const int idx = static_cast<int>(anim.samplers.size());
    anim.samplers.push_back(s);
    return idx;
}

// Root(0) -> Slew(1, rotation) -> Trolley(2, translation) -> Payload(3, translation, mesh),
// plus Slew -> Cable(4, scale, mesh). One clip, all channels LINEAR, 0..2s.
GltfDocument makeCraneLikeDoc()
{
    GltfDocument doc;

    GltfNode root;    root.name = "Root";    root.children = {1};
    doc.nodes.push_back(root);                                                    // 0
    GltfNode slew;    slew.name = "Slew";    slew.children = {2, 4};
    doc.nodes.push_back(slew);                                                    // 1
    GltfNode trolley; trolley.name = "Trolley"; trolley.translation = glm::vec3(2.f, 0.f, 0.f);
    trolley.children = {3};
    doc.nodes.push_back(trolley);                                                 // 2
    GltfNode payload; payload.name = "Payload"; payload.translation = glm::vec3(0.f, -1.f, 0.f);
    payload.meshIndex = 0;
    doc.nodes.push_back(payload);                                                 // 3
    GltfNode cable;   cable.name = "Cable";   cable.meshIndex = 0;   cable.children = {5};
    doc.nodes.push_back(cable);                                                   // 4
    GltfNode tip;     tip.name = "CableTip"; tip.translation = glm::vec3(0.f, -1.f, 0.f);
    tip.meshIndex = 0;
    doc.nodes.push_back(tip);                                                     // 5

    GltfScene scene; scene.nodes = {0}; doc.scenes.push_back(scene);
    doc.defaultScene = 0;
    GltfMesh mesh; doc.meshes.push_back(mesh);

    GltfAnimation anim;
    const int t = appendScalarTimes(doc, {0.f, 2.f});

    // Slew(1): identity -> +90deg about Y.
    anim.channels.push_back({appendQuatSampler(doc, anim, t,
        {glm::vec4(0, 0, 0, 1), glm::vec4(0, 0.70710678f, 0, 0.70710678f)}),
        {1, GltfAnimationPath::Rotation}});
    // Trolley(2): local X 2 -> 4.
    anim.channels.push_back({appendVec3Sampler(doc, anim, t,
        {glm::vec3(2, 0, 0), glm::vec3(4, 0, 0)}), {2, GltfAnimationPath::Translation}});
    // Payload(3): local Y -1 -> -3.
    anim.channels.push_back({appendVec3Sampler(doc, anim, t,
        {glm::vec3(0, -1, 0), glm::vec3(0, -3, 0)}), {3, GltfAnimationPath::Translation}});
    // Cable(4): local scale 1 -> 3 on Y.
    anim.channels.push_back({appendVec3Sampler(doc, anim, t,
        {glm::vec3(1, 1, 1), glm::vec3(1, 3, 1)}), {4, GltfAnimationPath::Scale}});

    doc.animations.push_back(anim);
    return doc;
}

} // namespace

TEST(GltfAnimationEvaluatorTest, NestedAnimatedNodesCompoundThroughTheChain)
{
    const GltfDocument doc = makeCraneLikeDoc();
    auto g = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 2.f);
    ASSERT_EQ(6u, g.size());

    // Slew is a pure +90deg-about-Y rotation: +X maps to -Z.
    const glm::vec3 mappedX = glm::vec3(g[1] * glm::vec4(1.f, 0.f, 0.f, 0.f));
    EXPECT_NEAR(0.f,  mappedX.x, 1e-4f);
    EXPECT_NEAR(-1.f, mappedX.z, 1e-4f);

    // Trolley local (4,0,0) under Slew's rotation -> global (0,0,-4).
    EXPECT_NEAR(0.f,  g[2][3][0], 1e-4f);
    EXPECT_NEAR(-4.f, g[2][3][2], 1e-4f);

    // Payload adds local (0,-3,0) (rotation about Y leaves Y untouched) -> global (0,-3,-4).
    EXPECT_NEAR(0.f,  g[3][3][0], 1e-4f);
    EXPECT_NEAR(-3.f, g[3][3][1], 1e-4f);
    EXPECT_NEAR(-4.f, g[3][3][2], 1e-4f);
}

namespace {

// Base(0) -> Tip(1), Tip at local (0,1,0). Skin joints {0,1} with bind-pose inverse bind
// matrices. Base has a rotation channel: identity -> +90deg about Z at t=2. Mirrors the
// soft-robot fixture (a root joint's rotation carries the whole chain through skinning).
GltfDocument makeSkinnedChainDoc()
{
    GltfDocument doc;

    GltfNode base; base.name = "Base"; base.children = {1};
    doc.nodes.push_back(base);                                        // 0
    GltfNode tip;  tip.name = "Tip";  tip.translation = glm::vec3(0.f, 1.f, 0.f); tip.meshIndex = 0;
    tip.skin = 0;
    doc.nodes.push_back(tip);                                         // 1

    GltfScene scene; scene.nodes = {0}; doc.scenes.push_back(scene);
    doc.defaultScene = 0;
    GltfMesh mesh; doc.meshes.push_back(mesh);

    GltfSkin skin;
    skin.joints = {0, 1};
    // Bind pose: Base at origin, Tip at (0,1,0). IBM = inverse(bindGlobal).
    skin.inverseBindMatrices = {
        glm::mat4(1.f),
        glm::inverse(glm::translate(glm::mat4(1.f), glm::vec3(0.f, 1.f, 0.f))),
    };
    doc.skins.push_back(skin);

    GltfAnimation anim;
    const int t = appendScalarTimes(doc, {0.f, 2.f});
    anim.channels.push_back({appendQuatSampler(doc, anim, t,
        {glm::vec4(0, 0, 0, 1), glm::vec4(0, 0, 0.70710678f, 0.70710678f)}),
        {0, GltfAnimationPath::Rotation}});
    doc.animations.push_back(anim);
    return doc;
}

} // namespace

TEST(GltfAnimationEvaluatorTest, SkinnedChainParentJointRotationCarriesChild)
{
    const GltfDocument doc = makeSkinnedChainDoc();

    // Bind pose: both skin matrices are identity (jointGlobal == bindGlobal).
    auto s0 = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, 0.f);
    ASSERT_EQ(2u, s0.size());
    EXPECT_NEAR(0.f, glm::length(glm::vec3(s0[1][3])), 1e-4f);

    // t=2: Base rotates +90deg about Z. A vertex at the tip's bind position (0,1,0) is skinned
    // by joint 1: skinMat1 * (0,1,0,1). skinMat1 = baseRot * tipLocal * ibm1, and
    // baseRot*tipLocal places the tip origin at baseRot*(0,1,0) = (-1,0,0); ibm1 first pulls the
    // vertex back to the tip's local frame, so the net effect on (0,1,0) is a move to (-1,0,0).
    auto s2 = GltfAnimationEvaluator::evaluateSkin(doc, 0, 0, 2.f);
    const glm::vec3 skinned = glm::vec3(s2[1] * glm::vec4(0.f, 1.f, 0.f, 1.f));
    EXPECT_NEAR(-1.f, skinned.x, 1e-4f);
    EXPECT_NEAR(0.f,  skinned.y, 1e-4f);
    EXPECT_NEAR(0.f,  skinned.z, 1e-4f);
}

TEST(GltfAnimationEvaluatorTest, ScaleChannelStretchesChildOffsets)
{
    const GltfDocument doc = makeCraneLikeDoc();

    // Cable(4) has only a Scale channel on Y; CableTip(5) sits at local (0,-1,0). The Slew
    // rotation is about Y, so it never touches the tip's Y -- only Cable's scale does.
    auto g0 = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 0.f);
    EXPECT_NEAR(1.f, g0[4][1][1], 1e-4f);       // scale identity at t=0
    EXPECT_NEAR(-1.f, g0[5][3][1], 1e-4f);      // tip 1 unit down

    auto g2 = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 2.f);
    EXPECT_NEAR(3.f, g2[4][1][1], 1e-4f);       // Y scaled 3x
    EXPECT_NEAR(-3.f, g2[5][3][1], 1e-4f);      // tip offset stretched to 3 units down

    // Halfway the scale is linearly interpolated to 2.
    auto g1 = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, 0, 1.f);
    EXPECT_NEAR(2.f,  g1[4][1][1], 1e-4f);
    EXPECT_NEAR(-2.f, g1[5][3][1], 1e-4f);
}

TEST(GltfAnimationEvaluatorTest, MorphDefaultsUseNodeThenMeshWithoutChannel)
{
    auto doc = makeDoc();
    doc.meshes.resize(1);
    doc.meshes[0].weights = {0.25f, 0.75f};
    doc.nodes[0].meshIndex = 0;
    EXPECT_EQ((std::vector<float>{0.25f, 0.75f}),
        GltfAnimationEvaluator::evaluateMorphWeights(doc, 0, 0, 2, 0.5f));
    doc.nodes[0].weights = {0.8f, 0.2f};
    EXPECT_EQ((std::vector<float>{0.8f, 0.2f}),
        GltfAnimationEvaluator::evaluateMorphWeights(doc, 0, 0, 2, 0.5f));
    EXPECT_EQ((std::vector<float>{0.8f, 0.2f}),
        GltfAnimationEvaluator::evaluateMorphWeights(doc, -1, 0, 2, 0.f));
}

TEST(GltfAnimationEvaluatorTest, SharedMeshNodesKeepIndependentMorphWeights)
{
    auto doc = makeDoc();
    doc.meshes.resize(1);
    doc.nodes[0].meshIndex = doc.nodes[1].meshIndex = 0;
    doc.nodes[0].weights = {0.8f, 0.2f};
    doc.nodes[1].weights = {0.9f, 0.9f};
    EXPECT_EQ((std::vector<float>{0.8f, 0.2f}),
        GltfAnimationEvaluator::evaluateMorphWeights(doc, 0, 0, 2, 0.5f));
    EXPECT_EQ((std::vector<float>{0.5f, 0.25f}),
        GltfAnimationEvaluator::evaluateMorphWeights(doc, 0, 1, 2, 0.5f));
}