#include "VulkanTestFixture.h"
#include "../VulkanRenderPass.h"

using Phantom::VKG::VulkanRenderPass;

using VulkanRenderPassTest = VulkanTestFixture;

TEST_F(VulkanRenderPassTest, CreateWithoutMsaa) {
    VulkanRenderPass rp;
    EXPECT_TRUE(rp.create(ctx_, VK_FORMAT_B8G8R8A8_UNORM, depthFormat_, VK_SAMPLE_COUNT_1_BIT));
    EXPECT_NE(rp.get(), VK_NULL_HANDLE);
    rp.destroy(ctx_.getDevice());
}

TEST_F(VulkanRenderPassTest, CreateWithMsaa) {
    VulkanRenderPass rp;
    EXPECT_TRUE(rp.create(ctx_, VK_FORMAT_B8G8R8A8_UNORM, depthFormat_, VK_SAMPLE_COUNT_2_BIT));
    EXPECT_NE(rp.get(), VK_NULL_HANDLE);
    EXPECT_EQ(rp.getSamples(), VK_SAMPLE_COUNT_2_BIT);
    rp.destroy(ctx_.getDevice());
}
