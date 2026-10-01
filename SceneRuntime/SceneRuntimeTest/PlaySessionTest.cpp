#include "gtest/gtest.h"

#include "../SceneRuntime/PlaySession.h"

using namespace Phantom::SceneRuntime;

namespace {

struct Probe {
    int captures = 0;
    int restores = 0;
    int steps = 0;
    double value = 0.0;    // "simulated" state: grows by dt per step
    double snapshot = 0.0;

    PlayHooks hooks()
    {
        PlayHooks h;
        h.capture = [this] { ++captures; snapshot = value; };
        h.restore = [this] { ++restores; value = snapshot; };
        h.step    = [this](double dt) { ++steps; value += dt; };
        return h;
    }
};

} // namespace

TEST(PlaySessionTest, StartsInEdit_ControlsAreNoOps)
{
    Probe p;
    PlaySession s;
    s.setHooks(p.hooks());
    s.pause();
    s.stop();
    s.reset();
    EXPECT_FALSE(s.step(0.0));
    EXPECT_EQ(s.advance(1.0), 0);
    EXPECT_TRUE(s.isEdit());
    EXPECT_EQ(p.captures, 0);
    EXPECT_EQ(p.restores, 0);
    EXPECT_EQ(p.steps, 0);
}

TEST(PlaySessionTest, PlayCapturesOnce_PauseResumeKeepsSnapshot)
{
    Probe p;
    PlaySession s;
    s.setHooks(p.hooks());
    s.play();
    EXPECT_TRUE(s.isPlaying());
    s.pause();
    EXPECT_TRUE(s.isPaused());
    EXPECT_TRUE(s.snapshotActive());
    s.play();
    EXPECT_TRUE(s.isPlaying());
    EXPECT_EQ(p.captures, 1);
}

TEST(PlaySessionTest, PauseDoesNotAdvance_StepDoes)
{
    Probe p;
    PlaySession s;
    s.setHooks(p.hooks());
    s.play();
    s.pause();
    EXPECT_EQ(s.advance(1.0), 0);
    EXPECT_EQ(p.steps, 0);
    EXPECT_TRUE(s.step(PlaySession::kFixedDt));
    EXPECT_EQ(p.steps, 1);
    EXPECT_TRUE(s.isPaused());
    EXPECT_NEAR(s.clock().sceneTime, PlaySession::kFixedDt, 1e-12);
}

TEST(PlaySessionTest, StepFromEdit_CapturesAndStaysPaused_StopUndoesIt)
{
    Probe p;
    p.value = 5.0;
    PlaySession s;
    s.setHooks(p.hooks());
    EXPECT_TRUE(s.step(0.5));
    EXPECT_TRUE(s.isPaused());
    EXPECT_DOUBLE_EQ(p.value, 5.5);
    s.stop();
    EXPECT_TRUE(s.isEdit());
    EXPECT_DOUBLE_EQ(p.value, 5.0);
    EXPECT_EQ(s.clock().stepCount, 0u);
}

TEST(PlaySessionTest, AdvanceUsesFixedSteps_IndependentOfFrameSplit)
{
    Probe a, b;
    PlaySession sa, sb;
    sa.setHooks(a.hooks());
    sb.setHooks(b.hooks());
    sa.play();
    sb.play();

    // Fifteen 1/30 s wall ticks vs. thirty 1/60 s ticks cover the same time -> the same steps.
    int total = 0;
    for (int i = 0; i < 15; ++i) total += sa.advance(1.0 / 30.0);
    for (int i = 0; i < 30; ++i) sb.advance(1.0 / 60.0);
    EXPECT_EQ(total, 30);
    EXPECT_EQ(a.steps, b.steps);
    EXPECT_NEAR(a.value, b.value, 1e-9);
    EXPECT_EQ(sa.clock().stepCount, sb.clock().stepCount);
}

TEST(PlaySessionTest, AdvanceCapsStepsPerCall_AndDropsBacklog)
{
    Probe p;
    PlaySession s;
    s.setHooks(p.hooks());
    s.play();
    EXPECT_EQ(s.advance(10.0), PlaySession::kMaxStepsPerAdvance);
    // No backlog is carried: a tiny follow-up tick runs nothing.
    EXPECT_EQ(s.advance(0.001), 0);
}

TEST(PlaySessionTest, StopRestoresAndResetsClock_ReplayIsIdentical)
{
    Probe p;
    p.value = 1.0;
    PlaySession s;
    s.setHooks(p.hooks());

    s.play();
    for (int i = 0; i < 10; ++i) s.step(PlaySession::kFixedDt);
    const double first = p.value;
    const double firstTime = s.clock().sceneTime;
    s.stop();
    EXPECT_DOUBLE_EQ(p.value, 1.0);
    EXPECT_DOUBLE_EQ(s.clock().sceneTime, 0.0);

    s.play();
    for (int i = 0; i < 10; ++i) s.step(PlaySession::kFixedDt);
    EXPECT_DOUBLE_EQ(p.value, first);
    EXPECT_DOUBLE_EQ(s.clock().sceneTime, firstTime);
}

TEST(PlaySessionTest, ResetRewindsAndStaysPaused)
{
    Probe p;
    p.value = 2.0;
    PlaySession s;
    s.setHooks(p.hooks());
    s.play();
    s.step(0.25);
    s.reset();
    EXPECT_TRUE(s.isPaused());
    EXPECT_DOUBLE_EQ(p.value, 2.0);
    EXPECT_DOUBLE_EQ(s.clock().sceneTime, 0.0);
    EXPECT_EQ(p.captures, 2); // once on Play, once re-captured by Reset
    s.stop();
    EXPECT_DOUBLE_EQ(p.value, 2.0);
}

TEST(PlaySessionTest, DiscardForgetsSnapshotWithoutRestoring)
{
    Probe p;
    PlaySession s;
    s.setHooks(p.hooks());
    s.play();
    s.step(0.5);
    s.discard();
    EXPECT_TRUE(s.isEdit());
    EXPECT_EQ(p.restores, 0);
    s.stop();
    EXPECT_EQ(p.restores, 0);
}

TEST(PlaySessionTest, ClockDomainsAdvanceTogether)
{
    Probe p;
    PlaySession s;
    s.setHooks(p.hooks());
    s.step(0.1);
    s.step(0.2);
    const auto& c = s.clock();
    EXPECT_NEAR(c.physicsTime, 0.3, 1e-12);
    EXPECT_NEAR(c.objectTime, 0.3, 1e-12);
    EXPECT_NEAR(c.skeletalTime, 0.3, 1e-12);
    EXPECT_NEAR(c.morphTime, 0.3, 1e-12);
    EXPECT_EQ(c.stepCount, 2u);
}

TEST(PlaySessionTest, ToStringNamesStates)
{
    EXPECT_STREQ(toString(PlayState::Edit), "Edit");
    EXPECT_STREQ(toString(PlayState::Playing), "Playing");
    EXPECT_STREQ(toString(PlayState::Paused), "Paused");
}
