#include <gtest/gtest.h>

#include "FrameRecording.h"

using namespace VKG;

TEST(FrameRecordingTest, ClearValues) {
    const auto v = swapchainClearValues();
    EXPECT_FLOAT_EQ(v[0].color.float32[0], 0.05f);
    EXPECT_FLOAT_EQ(v[0].color.float32[3], 1.f);
    EXPECT_FLOAT_EQ(v[1].depthStencil.depth, 1.f);
    EXPECT_EQ(v[1].depthStencil.stencil, 0u);
}

TEST(FrameRecordingTest, ViewportAndScissorCoverExtent) {
    const VkExtent2D e{1280, 720};
    const VkViewport vp = fullViewport(e);
    EXPECT_FLOAT_EQ(vp.width, 1280.f);
    EXPECT_FLOAT_EQ(vp.height, 720.f);
    EXPECT_FLOAT_EQ(vp.minDepth, 0.f);
    EXPECT_FLOAT_EQ(vp.maxDepth, 1.f);
    const VkRect2D sc = fullScissor(e);
    EXPECT_EQ(sc.offset.x, 0);
    EXPECT_EQ(sc.extent.width, 1280u);
    EXPECT_EQ(sc.extent.height, 720u);
}
