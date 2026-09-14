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

// Phase 4B render-graph vertical slice 1 (Universe): resize() must keep the SAME renderPass
// handle (so pipelines built against it earlier stay valid across a window resize) while
// producing fresh, correctly-sized images/framebuffer.
TEST_F(VulkanOffscreenTest, ResizeKeepsRenderPassAndRebuildsImages) {
    VulkanOffscreen offscreen;
    ASSERT_TRUE(offscreen.create(ctx_, 64, 64, VK_FORMAT_R8G8B8A8_UNORM, depthFormat_));

    const VkRenderPass originalRenderPass = offscreen.getRenderPass();

    EXPECT_TRUE(offscreen.resize(ctx_, 128, 256));

    EXPECT_TRUE(offscreen.isValid());
    EXPECT_EQ(offscreen.getRenderPass(), originalRenderPass); // unchanged -- the whole point
    // NOT asserted: that getColorImageView()/getFramebuffer() differ from before resize().
    // The old objects genuinely are destroyed and new ones created, but some drivers (Intel's
    // among them, observed running this suite) hand back the exact same opaque handle value
    // for the new allocation -- a valid implementation detail, not something callers may rely
    // on either way. What resize()'s caller (Renderer::setExtent()) actually depends on is that
    // it re-reads and re-writes these handles into any descriptor unconditionally every time,
    // never compares them to detect "did it change".
    EXPECT_NE(offscreen.getColorImageView(), VK_NULL_HANDLE);
    EXPECT_NE(offscreen.getDepthImageView(), VK_NULL_HANDLE);
    EXPECT_NE(offscreen.getFramebuffer(), VK_NULL_HANDLE);

    const VkExtent2D ext = offscreen.getExtent();
    EXPECT_EQ(ext.width, 128u);
    EXPECT_EQ(ext.height, 256u);

    offscreen.destroy(ctx_);
}

TEST_F(VulkanOffscreenTest, ResizeOnUncreatedOffscreenFails) {
    VulkanOffscreen offscreen;
    EXPECT_FALSE(offscreen.resize(ctx_, 64, 64)); // never created -- isValid() is false
}

TEST_F(VulkanOffscreenTest, ResizeToZeroFails) {
    VulkanOffscreen offscreen;
    ASSERT_TRUE(offscreen.create(ctx_, 64, 64, VK_FORMAT_R8G8B8A8_UNORM, depthFormat_));

    EXPECT_FALSE(offscreen.resize(ctx_, 0, 64));
    // A rejected resize (bad args) is not the "resize attempted and failed" case this
    // method's header comment describes -- the object is left exactly as it was, still valid,
    // at its previous size, since no destructive step was taken.
    EXPECT_TRUE(offscreen.isValid());
    EXPECT_EQ(offscreen.getExtent().width, 64u);

    offscreen.destroy(ctx_);
}
