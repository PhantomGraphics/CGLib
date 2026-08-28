#include "VulkanTestFixture.h"
#include "../VulkanCommandPool.h"

using VulkanCommandPoolTest = VulkanTestFixture;

// pool_ is already initialized by the fixture's SetUp().
TEST_F(VulkanCommandPoolTest, InitCreatesValidPool) {
    EXPECT_NE(pool_.get(), VK_NULL_HANDLE);
}

TEST_F(VulkanCommandPoolTest, AllocateAndFreeCommandBuffers) {
    auto bufs = pool_.allocateCommandBuffers(2);
    ASSERT_EQ(bufs.size(), 2u);
    EXPECT_NE(bufs[0], VK_NULL_HANDLE);
    EXPECT_NE(bufs[1], VK_NULL_HANDLE);

    pool_.freeCommandBuffers(bufs);
    EXPECT_TRUE(bufs.empty());
}

TEST_F(VulkanCommandPoolTest, SingleTimeCommandsRoundTrip) {
    VkCommandBuffer cmd = pool_.beginSingleTimeCommands();
    ASSERT_NE(cmd, VK_NULL_HANDLE);
    pool_.endSingleTimeCommands(cmd);
}
