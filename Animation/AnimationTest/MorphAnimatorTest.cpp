#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include "../Animation/MorphAnimator.h"
#include "../Animation/MorphTarget.h"
#include "../Animation/SkinnedMesh.h"

using namespace Phantom::Animation;

// -----------------------------------------------------------------------
// MorphAnimator::applyMorphs
// -----------------------------------------------------------------------

TEST(MorphAnimatorTest, ApplyZeroWeight_NoChange)
{
    SkinVertex v; v.position = {1.f, 2.f, 3.f};
    std::vector<SkinVertex> base = {v};

    MorphTarget t;
    t.name = "A";
    t.deltas.push_back({0, glm::vec3{10.f, 0.f, 0.f}});

    MorphState state;
    state.weights = {0.f};

    std::vector<SkinVertex> out;
    MorphAnimator::applyMorphs(base, {t}, state, out);

    ASSERT_EQ(1u, out.size());
    EXPECT_FLOAT_EQ(1.f, out[0].position.x);
}

TEST(MorphAnimatorTest, ApplyFullWeight_FullDelta)
{
    SkinVertex v; v.position = {0.f, 0.f, 0.f};
    std::vector<SkinVertex> base = {v};

    MorphTarget t;
    t.name = "Smile";
    t.deltas.push_back({0, glm::vec3{2.f, 1.f, -1.f}});

    MorphState state;
    state.weights = {1.f};

    std::vector<SkinVertex> out;
    MorphAnimator::applyMorphs(base, {t}, state, out);

    ASSERT_EQ(1u, out.size());
    EXPECT_FLOAT_EQ( 2.f, out[0].position.x);
    EXPECT_FLOAT_EQ( 1.f, out[0].position.y);
    EXPECT_FLOAT_EQ(-1.f, out[0].position.z);
}

TEST(MorphAnimatorTest, ApplyHalfWeight_HalfDelta)
{
    SkinVertex v; v.position = {0.f, 0.f, 0.f};
    std::vector<SkinVertex> base = {v};

    MorphTarget t;
    t.deltas.push_back({0, glm::vec3{1.f, 0.f, 0.f}});

    MorphState state;
    state.weights = {0.5f};

    std::vector<SkinVertex> out;
    MorphAnimator::applyMorphs(base, {t}, state, out);

    EXPECT_NEAR(0.5f, out[0].position.x, 1e-6f);
}

TEST(MorphAnimatorTest, MultipleMorphs_Additive)
{
    SkinVertex v; v.position = {0.f, 0.f, 0.f};
    std::vector<SkinVertex> base = {v};

    MorphTarget t1, t2;
    t1.deltas.push_back({0, glm::vec3{1.f, 0.f, 0.f}});
    t2.deltas.push_back({0, glm::vec3{0.f, 2.f, 0.f}});

    MorphState state;
    state.weights = {1.f, 1.f};

    std::vector<SkinVertex> out;
    MorphAnimator::applyMorphs(base, {t1, t2}, state, out);

    EXPECT_FLOAT_EQ(1.f, out[0].position.x);
    EXPECT_FLOAT_EQ(2.f, out[0].position.y);
}

TEST(MorphAnimatorTest, BaseMeshNotModified)
{
    SkinVertex v; v.position = {5.f, 5.f, 5.f};
    std::vector<SkinVertex> base = {v};
    const auto baseCopy = base;

    MorphTarget t;
    t.deltas.push_back({0, glm::vec3{100.f, 0.f, 0.f}});

    MorphState state;
    state.weights = {1.f};

    std::vector<SkinVertex> out;
    MorphAnimator::applyMorphs(base, {t}, state, out);

    EXPECT_FLOAT_EQ(baseCopy[0].position.x, base[0].position.x);
}

// -----------------------------------------------------------------------
// MorphAnimator::update (weight interpolation)
// -----------------------------------------------------------------------

TEST(MorphAnimatorTest, UpdateEmptyClip_ZeroWeights)
{
    MorphTarget t; t.name = "A";
    MorphAnimationClip clip;
    MorphState state;

    MorphAnimator anim;
    anim.update({t}, clip, 0.5f, state);

    ASSERT_EQ(1u, state.weights.size());
    EXPECT_FLOAT_EQ(0.f, state.weights[0]);
}

TEST(MorphAnimatorTest, UpdateLinearInterpolation)
{
    MorphTarget t; t.name = "A";

    MorphChannel ch;
    ch.morphIndex = 0;
    ch.keyframes  = {{0.f, 0.f}, {1.f, 1.f}};

    MorphAnimationClip clip;
    clip.channels = {ch};
    clip.duration = 1.f;

    MorphState state;
    MorphAnimator anim;
    anim.update({t}, clip, 0.5f, state);

    EXPECT_NEAR(0.5f, state.weights[0], 1e-5f);
}

TEST(MorphAnimatorTest, UpdateBeforeFirstKeyframe_UsesFirstWeight)
{
    MorphTarget t; t.name = "A";

    MorphChannel ch;
    ch.morphIndex = 0;
    ch.keyframes  = {{1.f, 0.8f}, {2.f, 1.f}};

    MorphAnimationClip clip;
    clip.channels = {ch};
    clip.duration = 2.f;

    MorphState state;
    MorphAnimator anim;
    anim.update({t}, clip, 0.f, state);

    EXPECT_FLOAT_EQ(0.8f, state.weights[0]);
}

TEST(MorphAnimatorTest, UpdateAfterLastKeyframe_UsesLastWeight)
{
    MorphTarget t; t.name = "A";

    MorphChannel ch;
    ch.morphIndex = 0;
    ch.keyframes  = {{0.f, 0.f}, {1.f, 0.6f}};

    MorphAnimationClip clip;
    clip.channels = {ch};
    clip.duration = 1.f;

    MorphState state;
    MorphAnimator anim;
    anim.update({t}, clip, 5.f, state);

    EXPECT_FLOAT_EQ(0.6f, state.weights[0]);
}

TEST(MorphAnimatorTest, UpdateMultipleChannels)
{
    MorphTarget t0, t1;
    t0.name = "A"; t1.name = "B";

    MorphChannel ch0, ch1;
    ch0.morphIndex = 0; ch0.keyframes = {{0.f, 0.2f}, {1.f, 0.8f}};
    ch1.morphIndex = 1; ch1.keyframes = {{0.f, 1.f},  {1.f, 0.f}};

    MorphAnimationClip clip;
    clip.channels = {ch0, ch1};
    clip.duration = 1.f;

    MorphState state;
    MorphAnimator anim;
    anim.update({t0, t1}, clip, 0.5f, state);

    EXPECT_NEAR(0.5f, state.weights[0], 1e-5f); // (0.2 + 0.8) / 2
    EXPECT_NEAR(0.5f, state.weights[1], 1e-5f); // (1.0 + 0.0) / 2
}
