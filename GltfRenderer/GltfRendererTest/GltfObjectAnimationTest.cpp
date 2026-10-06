#include "gtest/gtest.h"

#include "../Gltf/GltfAccessorBuilder.h"
#include "../Renderer/GltfObjectAnimation.h"

using namespace Phantom::Gltf;

namespace {

// Nodes: 0 (translated by the clip) -> child 1 -> child 2; node 3 is unrelated and only has a
// Weights channel; node 4 is unrelated and untouched. Clip 0 moves 0; clip 1 is Weights-only.
GltfDocument makeDoc()
{
    GltfDocument doc;
    doc.nodes.resize(5);
    doc.nodes[0].children = {1};
    doc.nodes[1].children = {2};
    GltfScene scene;
    scene.nodes = {0, 3, 4};
    doc.scenes.push_back(scene);
    doc.defaultScene = 0;

    auto addClip = [&](int node, GltfAnimationPath path) {
        GltfAnimation anim;
        std::vector<float> times = {0.f, 2.f};
        GltfAnimationSampler sampler;
        sampler.input = appendAccessor(doc, times, GltfComponentType::Float, GltfAccessorType::Scalar);
        if (path == GltfAnimationPath::Weights) {
            std::vector<float> w = {0.f, 1.f};
            sampler.output = appendAccessor(doc, w, GltfComponentType::Float, GltfAccessorType::Scalar);
        } else {
            std::vector<glm::vec3> v = {glm::vec3(0.f), glm::vec3(4.f, 0.f, 0.f)};
            sampler.output = appendAccessor(doc, v, GltfComponentType::Float, GltfAccessorType::Vec3);
        }
        anim.samplers.push_back(sampler);
        anim.channels.push_back({0, {node, path}});
        doc.animations.push_back(anim);
    };
    addClip(0, GltfAnimationPath::Translation);
    addClip(3, GltfAnimationPath::Weights);
    return doc;
}

} // namespace

TEST(GltfObjectAnimationTest, NoDocumentIsInert)
{
    GltfObjectAnimation a;
    a.reset(nullptr);
    a.setClip(0);
    EXPECT_EQ(-1, a.clip());
    EXPECT_EQ(0, a.clipCount());
    EXPECT_FLOAT_EQ(0.f, a.duration(0));
    EXPECT_FALSE(a.consumeDirty());
    EXPECT_TRUE(a.evaluateGlobals().empty());
}

TEST(GltfObjectAnimationTest, ClipOutOfRangeDisables)
{
    const GltfDocument doc = makeDoc();
    GltfObjectAnimation a;
    a.reset(&doc);
    EXPECT_EQ(2, a.clipCount());

    a.setClip(7);
    EXPECT_EQ(-1, a.clip());
    EXPECT_FALSE(a.consumeDirty()); // already -1: nothing changed

    a.setClip(0);
    EXPECT_EQ(0, a.clip());
    a.setClip(-1);
    EXPECT_EQ(-1, a.clip());
    EXPECT_TRUE(a.evaluateGlobals().empty());
}

TEST(GltfObjectAnimationTest, DirtyIsConsumedOnce)
{
    const GltfDocument doc = makeDoc();
    GltfObjectAnimation a;
    a.reset(&doc);

    a.setClip(0);
    EXPECT_TRUE(a.consumeDirty());
    EXPECT_FALSE(a.consumeDirty());

    a.setTime(1.f);
    EXPECT_TRUE(a.consumeDirty());
    a.setTime(1.f); // same value: not dirty
    EXPECT_FALSE(a.consumeDirty());
}

TEST(GltfObjectAnimationTest, TranslationMarksWholeSubtreeOnly)
{
    const GltfDocument doc = makeDoc();
    GltfObjectAnimation a;
    a.reset(&doc);
    a.setClip(0);

    EXPECT_TRUE(a.isNodeAnimated(0));
    EXPECT_TRUE(a.isNodeAnimated(1)); // child of an animated node
    EXPECT_TRUE(a.isNodeAnimated(2)); // grandchild
    EXPECT_FALSE(a.isNodeAnimated(3));
    EXPECT_FALSE(a.isNodeAnimated(4));
    EXPECT_FALSE(a.isNodeAnimated(-1));
    EXPECT_FALSE(a.isNodeAnimated(99));
}

TEST(GltfObjectAnimationTest, WeightsChannelDoesNotMoveNode)
{
    const GltfDocument doc = makeDoc();
    GltfObjectAnimation a;
    a.reset(&doc);
    a.setClip(1);
    for (int n = 0; n < 5; ++n) EXPECT_FALSE(a.isNodeAnimated(n)) << n;
}

TEST(GltfObjectAnimationTest, EvaluatesGlobalsAtTime)
{
    const GltfDocument doc = makeDoc();
    GltfObjectAnimation a;
    a.reset(&doc);
    a.setClip(0);
    a.setTime(1.f); // halfway through the 2 s clip

    const auto globals = a.evaluateGlobals();
    ASSERT_EQ(5u, globals.size());
    EXPECT_NEAR(2.f, globals[0][3][0], 1e-5f);
    EXPECT_NEAR(2.f, globals[2][3][0], 1e-5f); // inherited through the chain
    EXPECT_NEAR(0.f, globals[4][3][0], 1e-5f);
    EXPECT_NEAR(2.f, a.duration(0), 1e-5f);
}

TEST(GltfObjectAnimationTest, ResetClearsState)
{
    const GltfDocument doc = makeDoc();
    GltfObjectAnimation a;
    a.reset(&doc);
    a.setClip(0);
    a.setTime(1.f);
    a.reset(&doc);
    EXPECT_EQ(-1, a.clip());
    EXPECT_FLOAT_EQ(0.f, a.time());
    EXPECT_FALSE(a.consumeDirty());
    EXPECT_FALSE(a.isNodeAnimated(0));
}
