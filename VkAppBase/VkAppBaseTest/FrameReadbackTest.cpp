#include <gtest/gtest.h>

#include "FrameReadback.h"

using VKG::FrameReadback;

TEST(FrameReadbackTest, ScreenshotOnlyOnConfiguredFrameAndOnce) {
    FrameReadback r;
    EXPECT_FALSE(r.beginFrame(0).screenshot);  // no path
    r.setScreenshot("a.png", 3);
    EXPECT_FALSE(r.beginFrame(2).screenshot);
    EXPECT_TRUE(r.beginFrame(3).screenshot);
    EXPECT_FALSE(r.beginFrame(4).screenshot);
    EXPECT_FALSE(r.screenshotDone());
}

TEST(FrameReadbackTest, DefaultFrameOnlyAppliesWhenUnset) {
    FrameReadback r;
    r.setScreenshotDefaultFrame(5);
    EXPECT_EQ(r.screenshotFrame(), 5);
    r.setScreenshotDefaultFrame(9);
    EXPECT_EQ(r.screenshotFrame(), 5);
}

TEST(FrameReadbackTest, PixelRequestLifecycle) {
    FrameReadback r;
    uint8_t out[4];
    EXPECT_FALSE(r.beginFrame(0).pixel);
    EXPECT_FALSE(r.pollPixel(out));
    r.requestPixel(1, 2);
    EXPECT_TRUE(r.beginFrame(0).pixel);
    EXPECT_FALSE(r.pollPixel(out));  // not finished yet
}

TEST(FrameReadbackTest, ToRgbaSwapsOnlyBgra) {
    const uint8_t src[4] = {1, 2, 3, 4};
    uint8_t out[4];
    FrameReadback::toRgba(src, VK_FORMAT_B8G8R8A8_UNORM, out);
    EXPECT_EQ(out[0], 3); EXPECT_EQ(out[1], 2); EXPECT_EQ(out[2], 1); EXPECT_EQ(out[3], 4);
    FrameReadback::toRgba(src, VK_FORMAT_R8G8B8A8_UNORM, out);
    EXPECT_EQ(out[0], 1); EXPECT_EQ(out[2], 3);
}

TEST(FrameReadbackTest, ClampPixel) {
    auto p = FrameReadback::clampPixel(100, 200, {64, 32});
    EXPECT_EQ(p.x, 63); EXPECT_EQ(p.y, 31);
    p = FrameReadback::clampPixel(5, 6, {64, 32});
    EXPECT_EQ(p.x, 5); EXPECT_EQ(p.y, 6);
    p = FrameReadback::clampPixel(5, 6, {0, 0});
    EXPECT_EQ(p.x, 0); EXPECT_EQ(p.y, 0);
}
