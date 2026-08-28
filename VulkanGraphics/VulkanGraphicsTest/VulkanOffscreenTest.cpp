#include "VulkanTestFixture.h"
#include "../VulkanOffscreen.h"

using Phantom::VKG::VulkanOffscreen;

using VulkanOffscreenTest = VulkanTestFixture;

TEST_F(VulkanOffscreenTest, CreateIsValid) {
    VulkanOffscreen offscreen;
    EXPECT_TRUE(offscreen.create(ctx_, 64, 64, VK_FORMAT_R8G8B8A8_UNORM, depthFormat_));

    EXPECT_TRUE(offscreen.isValid());
    EXPECT_NE(offscreen.getFramebuffer(), VK_NULL_HANDLE);
    EXPECT_NE(offscreen.getColorImageView(), VK_NULL_HANDLE);
    EXPECT_NE(offscreen.getDepthImageView(), VK_NULL_HANDLE);

    offscreen.destroy(ctx_);
}
