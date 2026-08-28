#include "VulkanTestFixture.h"

using VulkanContextTest = VulkanTestFixture;

TEST_F(VulkanContextTest, InstanceAndDeviceAreValid) {
    EXPECT_NE(ctx_.getInstance(), VK_NULL_HANDLE);
    EXPECT_NE(ctx_.getPhysicalDevice(), VK_NULL_HANDLE);
    EXPECT_NE(ctx_.getDevice(), VK_NULL_HANDLE);
    EXPECT_NE(ctx_.getGraphicsQueue(), VK_NULL_HANDLE);
    EXPECT_NE(ctx_.getPresentQueue(), VK_NULL_HANDLE);
    EXPECT_NE(ctx_.getAllocator(), nullptr);
}

TEST_F(VulkanContextTest, FindMemoryTypeWithPlausibleFilter) {
    // typeFilter with every bit set accepts any memory type index the
    // property-flag search happens to land on.
    auto idx = ctx_.findMemoryType(0xFFFFFFFFu, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ASSERT_TRUE(idx.has_value());
    EXPECT_LT(*idx, 32u);
}

// Abnormal path: typeFilter=0 rejects every memory type index, so no
// property-flag combination can ever match. Phase 1: findMemoryType()
// returns std::nullopt instead of throwing (Phase 0 recorded EXPECT_THROW).
TEST_F(VulkanContextTest, FindMemoryTypeWithImpossibleFilterReturnsNullopt) {
    auto idx = ctx_.findMemoryType(0u, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    EXPECT_FALSE(idx.has_value());
}
