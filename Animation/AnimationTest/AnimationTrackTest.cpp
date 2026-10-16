#include <gtest/gtest.h>

#include "../Core/AnimationTrack.h"

#include <array>
#include <cmath>
#include <limits>

// Track data and math moved from PhantomStudio (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 1).
using namespace Phantom::Animation;

namespace {

TrackKey key(int frame, float x, float y, float z, KeyInterp interp = KeyInterp::Linear) {
    TrackKey k;
    k.frame = frame;
    k.value = { x, y, z };
    k.interp = interp;
    return k;
}

// Columns 0 and 2 of Ry*Rx*Rz (the model matrix order): enough to pin down the whole rotation.
std::array<double, 6> poseColumns(const std::array<float, 3>& e) {
    const double d = 3.14159265358979323846 / 180.0;
    const double cx = std::cos(e[0] * d), sx = std::sin(e[0] * d);
    const double cy = std::cos(e[1] * d), sy = std::sin(e[1] * d);
    const double cz = std::cos(e[2] * d), sz = std::sin(e[2] * d);
    return { cy * cz + sy * sx * sz, cx * sz, -sy * cz + cy * sx * sz,
             sy * cx, -sx, cy * cx };
}

} // namespace

TEST(AnimationTrack, HoldsEndsAndInterpolatesLinearly) {
    AnimTracks t;
    insertKey(t.track(AnimChannel::Translation), key(10, 0, 0, 0));
    insertKey(t.track(AnimChannel::Translation), key(20, 10, 20, 30));
    TrackTransform pose;
    pose.scale = { 2, 2, 2 };  // unkeyed channel keeps the base value
    evaluateTracks(t, 0, pose);
    EXPECT_FLOAT_EQ(0.f, pose.translation[0]);  // before the first key: held
    evaluateTracks(t, 15, pose);
    EXPECT_FLOAT_EQ(5.f, pose.translation[0]);
    EXPECT_FLOAT_EQ(15.f, pose.translation[2]);
    evaluateTracks(t, 15.5, pose);
    EXPECT_NEAR(5.5f, pose.translation[0], 1e-5f);  // sub-frame
    evaluateTracks(t, 99, pose);
    EXPECT_FLOAT_EQ(30.f, pose.translation[2]);  // after the last key: held
    EXPECT_FLOAT_EQ(2.f, pose.scale[0]);
}

TEST(AnimationTrack, StepHoldsUntilNextKey) {
    AnimTracks t;
    insertKey(t.track(AnimChannel::Scale), key(0, 1, 1, 1, KeyInterp::Step));
    insertKey(t.track(AnimChannel::Scale), key(10, 3, 3, 3));
    TrackTransform pose;
    evaluateTracks(t, 9.9, pose);
    EXPECT_FLOAT_EQ(1.f, pose.scale[0]);
    evaluateTracks(t, 10, pose);
    EXPECT_FLOAT_EQ(3.f, pose.scale[0]);
}

TEST(AnimationTrack, EaseCurvesStartAndEndSlowlyAndKeepTheEndpoints) {
    for (KeyInterp mode : { KeyInterp::EaseIn, KeyInterp::EaseOut, KeyInterp::EaseInOut }) {
        AnimTracks t;
        insertKey(t.track(AnimChannel::Translation), key(0, 0, 0, 0, mode));
        insertKey(t.track(AnimChannel::Translation), key(10, 10, 0, 0));
        TrackTransform pose;
        evaluateTracks(t, 0, pose);
        EXPECT_FLOAT_EQ(0.f, pose.translation[0]);
        evaluateTracks(t, 10, pose);
        EXPECT_FLOAT_EQ(10.f, pose.translation[0]);
        evaluateTracks(t, 2.5, pose);
        const float quarter = pose.translation[0];
        if (mode == KeyInterp::EaseIn) EXPECT_NEAR(0.625f, quarter, 1e-4f);        // 10 * 0.25^2
        if (mode == KeyInterp::EaseOut) EXPECT_NEAR(4.375f, quarter, 1e-4f);       // 10 * (1 - 0.75^2)
        if (mode == KeyInterp::EaseInOut) EXPECT_NEAR(1.5625f, quarter, 1e-4f);    // 10 * smoothstep(0.25)
        evaluateTracks(t, 5, pose);
        EXPECT_NEAR(mode == KeyInterp::EaseIn ? 2.5f : mode == KeyInterp::EaseOut ? 7.5f : 5.f, pose.translation[0], 1e-4f);
    }
}

TEST(AnimationTrack, InterpNamesRoundTrip) {
    for (KeyInterp mode : { KeyInterp::Step, KeyInterp::Linear, KeyInterp::EaseIn, KeyInterp::EaseOut, KeyInterp::EaseInOut, KeyInterp::Bezier }) {
        KeyInterp parsed = KeyInterp::Step;
        ASSERT_TRUE(parseInterpName(interpName(mode), parsed));
        EXPECT_EQ(mode, parsed);
    }
    KeyInterp parsed = KeyInterp::Step;
    EXPECT_TRUE(parseInterpName("Ease-In-Out", parsed));
    EXPECT_EQ(KeyInterp::EaseInOut, parsed);
    EXPECT_TRUE(parseInterpName("Bezier", parsed));
    EXPECT_EQ(KeyInterp::Bezier, parsed);
    EXPECT_FALSE(parseInterpName("wobble", parsed));
}

TEST(AnimationTrack, BezierTimingFollowsTheHandles) {
    // linear handles (on the diagonal) reproduce linear time; the default handles are the symmetric ease-in-out
    const BezierHandles diagonal = { 1.f / 3.f, 1.f / 3.f, 2.f / 3.f, 2.f / 3.f };
    for (double t : { 0.0, 0.1, 0.5, 0.9, 1.0 }) EXPECT_NEAR(t, bezierTime(diagonal, t), 1e-6);
    EXPECT_NEAR(0.5, bezierTime(kDefaultBezier, 0.5), 1e-6);
    EXPECT_LT(bezierTime(kDefaultBezier, 0.2), 0.2);
    EXPECT_GT(bezierTime(kDefaultBezier, 0.8), 0.8);
    // y beyond 1 overshoots the end value before settling
    const BezierHandles overshoot = { 0.5f, 1.5f, 0.75f, 1.f };
    EXPECT_GT(bezierTime(overshoot, 0.5), 1.0);
    // the curve passes through (0,0) and (1,1) whatever the handles
    EXPECT_DOUBLE_EQ(0.0, bezierTime(overshoot, 0.0));
    EXPECT_DOUBLE_EQ(1.0, bezierTime(overshoot, 1.0));

    EXPECT_TRUE(validBezier(kDefaultBezier));
    EXPECT_FALSE(validBezier({ -0.1f, 0.f, 0.5f, 1.f }));
    EXPECT_FALSE(validBezier({ 0.5f, 0.f, 1.5f, 1.f }));
    EXPECT_FALSE(validBezier({ 0.5f, std::numeric_limits<float>::quiet_NaN(), 0.5f, 1.f }));

    AnimTracks t;
    TrackKey a = key(0, 0, 0, 0, KeyInterp::Bezier);
    a.bezier = { 0.f, 0.f, 0.5f, 0.f };   // starts at once, then almost stands still until halfway
    insertKey(t.track(AnimChannel::Translation), a);
    insertKey(t.track(AnimChannel::Translation), key(10, 10, 0, 0));
    TrackTransform pose;
    evaluateTracks(t, 5, pose);
    EXPECT_NEAR(10.0 * bezierTime(a.bezier, 0.5), pose.translation[0], 1e-4);
    evaluateTracks(t, 10, pose);
    EXPECT_FLOAT_EQ(10.f, pose.translation[0]);
}

TEST(AnimationTrack, EulerQuaternionRoundTripKeepsThePose) {
    const std::array<float, 3> cases[] = { {10, 20, 30}, {-45, 170, 5}, {80, -120, 60}, {0, 0, 0}, {0, 90, 0} };
    for (const auto& e : cases) {
        const auto back = quaternionToEuler(eulerToQuaternion(e));
        const auto a = poseColumns(e), b = poseColumns(back);
        for (int i = 0; i < 6; ++i) EXPECT_NEAR(a[i], b[i], 1e-5) << e[0] << "," << e[1] << "," << e[2];
    }
}

TEST(AnimationTrack, QuaternionRotationTakesTheShortestPath) {
    AnimTracks t;
    insertKey(t.track(AnimChannel::Rotation), key(0, 0, 350, 0));
    insertKey(t.track(AnimChannel::Rotation), key(10, 0, 10, 0));
    TrackTransform pose;
    evaluateTracks(t, 5, pose);
    // 350 -> 10 is a 20 degree turn through 0, not 340 degrees back through 180.
    EXPECT_NEAR(0.0, std::sin(pose.rotationEulerDeg[1] * 3.14159265358979 / 180.0), 1e-4);
    EXPECT_NEAR(1.0, std::cos(pose.rotationEulerDeg[1] * 3.14159265358979 / 180.0), 1e-4);
    evaluateTracks(t, 0, pose);
    EXPECT_FLOAT_EQ(350.f, pose.rotationEulerDeg[1]);  // key frames return the authored value exactly
    evaluateTracks(t, 10, pose);
    EXPECT_FLOAT_EQ(10.f, pose.rotationEulerDeg[1]);
}

TEST(AnimationTrack, LegacyEulerModeBlendsPerComponent) {
    AnimTracks t;
    t.rotationMode = RotationMode::LegacyEuler;
    insertKey(t.track(AnimChannel::Rotation), key(0, 0, 350, 0));
    insertKey(t.track(AnimChannel::Rotation), key(10, 0, 10, 0));
    TrackTransform pose;
    evaluateTracks(t, 5, pose);
    EXPECT_FLOAT_EQ(180.f, pose.rotationEulerDeg[1]);  // what files before v10 did
    EXPECT_TRUE(convertRotationToQuaternion(t));
    EXPECT_FALSE(convertRotationToQuaternion(t));
}

TEST(AnimationTrack, MoveKeysRefusesCollisionsAndLeavesTracksUntouched) {
    AnimTracks t;
    insertKey(t.track(AnimChannel::Translation), key(0, 1, 0, 0));
    insertKey(t.track(AnimChannel::Translation), key(10, 2, 0, 0));
    std::string error;
    int count = -1;
    EXPECT_FALSE(shiftKeys(t, kAllChannels, { 0 }, 10, false, false, &error, &count));
    EXPECT_NE(error.find("frame 10"), std::string::npos);
    ASSERT_EQ(2u, t.track(AnimChannel::Translation).size());
    EXPECT_EQ(0, t.track(AnimChannel::Translation)[0].frame);
    EXPECT_EQ(0, count);

    EXPECT_FALSE(shiftKeys(t, kAllChannels, { 0 }, -1, false, true, &error, &count));  // below frame 0
    ASSERT_EQ(2u, t.track(AnimChannel::Translation).size());

    // Moving both keys together never collides with each other.
    EXPECT_TRUE(shiftKeys(t, kAllChannels, { 0, 10 }, 5, false, false, &error, &count));
    EXPECT_EQ(2, count);
    EXPECT_EQ(5, t.track(AnimChannel::Translation)[0].frame);
    EXPECT_EQ(15, t.track(AnimChannel::Translation)[1].frame);

    // Overwrite replaces the key that sits on the destination.
    EXPECT_TRUE(shiftKeys(t, kAllChannels, { 5 }, 10, true, true, &error, &count));  // copy 5 -> 15
    ASSERT_EQ(2u, t.track(AnimChannel::Translation).size());
    EXPECT_FLOAT_EQ(1.f, t.track(AnimChannel::Translation)[1].value[0]);
}

TEST(AnimationTrack, ChannelMaskParsing) {
    unsigned mask = 0;
    std::string error;
    EXPECT_TRUE(parseChannelMask("translation,rotation", mask, &error));
    EXPECT_EQ(channelBit(AnimChannel::Translation) | channelBit(AnimChannel::Rotation), mask);
    EXPECT_TRUE(parseChannelMask("all", mask, &error));
    EXPECT_EQ(kAllChannels, mask);
    EXPECT_FALSE(parseChannelMask("tilt", mask, &error));
}

TEST(ClipPlacement, DefaultsReproduceTheUnplacedClip) {
    ClipPlacement p;
    EXPECT_TRUE(p.isDefault());
    EXPECT_DOUBLE_EQ(0.0, clipLocalTime(p, 0.0, 30, 2.0));
    EXPECT_DOUBLE_EQ(1.5, clipLocalTime(p, 1.5, 30, 2.0));
    EXPECT_DOUBLE_EQ(2.0, clipLocalTime(p, 9.0, 30, 2.0));  // holds the last pose past the end
}

TEST(ClipPlacement, StartSpeedTrimAndLoop) {
    ClipPlacement p;
    p.startFrame = 30;  // starts at 1 s
    EXPECT_DOUBLE_EQ(0.0, clipLocalTime(p, 0.5, 30, 2.0));  // before the start: the first pose
    EXPECT_DOUBLE_EQ(0.5, clipLocalTime(p, 1.5, 30, 2.0));
    p.speed = 2.0;
    EXPECT_DOUBLE_EQ(1.0, clipLocalTime(p, 1.5, 30, 2.0));
    p.startFrame = 0;
    p.speed = 1.0;
    p.trimIn = 0.5;
    p.trimOut = 1.5;  // plays 0.5 .. 1.5
    EXPECT_DOUBLE_EQ(0.5, clipLocalTime(p, 0.0, 30, 2.0));
    EXPECT_DOUBLE_EQ(1.0, clipLocalTime(p, 0.5, 30, 2.0));
    EXPECT_DOUBLE_EQ(1.5, clipLocalTime(p, 5.0, 30, 2.0));  // clamped to the trim end
    p.loop = true;
    EXPECT_NEAR(0.75, clipLocalTime(p, 1.25, 30, 2.0), 1e-12);  // wraps inside the 1 s window
    p.trimOut = 9.0;  // beyond the clip: limited to its length
    EXPECT_NEAR(0.5 + 0.5, clipLocalTime(p, 2.0, 30, 2.0), 1e-12);
}
