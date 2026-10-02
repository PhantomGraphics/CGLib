#include "gtest/gtest.h"

#include "../SceneRuntime/ClipPlayback.h"

using namespace Phantom::SceneRuntime;

TEST(ClipPlaybackTest, StoppedOrNoClipDoesNothing)
{
    ClipState s;
    advanceClip(s, 1.f, 2.f);
    EXPECT_FLOAT_EQ(s.time, 0.f);
    s.clip = 0;
    advanceClip(s, 1.f, 2.f); // not playing
    EXPECT_FLOAT_EQ(s.time, 0.f);
}

TEST(ClipPlaybackTest, LoopWrapsForwardAndBackward)
{
    ClipState s{ 0, 1.5f, 1.f, true, true };
    advanceClip(s, 1.f, 2.f);
    EXPECT_NEAR(s.time, 0.5f, 1e-6);
    s.speed = -1.f;
    advanceClip(s, 1.f, 2.f);
    EXPECT_NEAR(s.time, 1.5f, 1e-6);
    EXPECT_TRUE(s.playing);
}

TEST(ClipPlaybackTest, NonLoopStopsAtEnds)
{
    ClipState s{ 0, 1.5f, 1.f, false, true };
    advanceClip(s, 1.f, 2.f);
    EXPECT_FLOAT_EQ(s.time, 2.f);
    EXPECT_FALSE(s.playing);

    ClipState b{ 0, 0.5f, -1.f, false, true };
    advanceClip(b, 1.f, 2.f);
    EXPECT_FLOAT_EQ(b.time, 0.f);
    EXPECT_FALSE(b.playing);
}

TEST(ClipPlaybackTest, ZeroDurationHoldsAtZero)
{
    ClipState s{ 0, 0.3f, 1.f, false, true };
    advanceClip(s, 1.f, 0.f);
    EXPECT_FLOAT_EQ(s.time, 0.f);
    EXPECT_FALSE(s.playing);
}
