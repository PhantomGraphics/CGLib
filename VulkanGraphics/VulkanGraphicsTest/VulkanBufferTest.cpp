#include "VulkanTestFixture.h"
#include "../VulkanBuffer.h"

#include <array>

using Phantom::VKG::VulkanBuffer;

using VulkanBufferTest = VulkanTestFixture;

TEST_F(VulkanBufferTest, CreateDeviceLocalWithInitialData) {
    std::array<float, 4> data{1.f, 2.f, 3.f, 4.f};

    VulkanBuffer buf;
    EXPECT_TRUE(buf.create(ctx_, pool_, sizeof(data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, data.data()));

    EXPECT_TRUE(buf.isValid());
    EXPECT_EQ(buf.getSize(), sizeof(data));

    buf.destroy();
}

TEST_F(VulkanBufferTest, CreateMappedAndWrite) {
    VulkanBuffer ubo;
    EXPECT_TRUE(ubo.createMapped(ctx_, sizeof(float) * 4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));

    EXPECT_TRUE(ubo.isValid());
    ASSERT_NE(ubo.getMapped(), nullptr);

    float value[4] = {1.f, 2.f, 3.f, 4.f};
    ubo.write(value, sizeof(value));

    ubo.destroy();
}

// Abnormal path: vmaCreateBuffer fails for a 1 TB device-local allocation.
// Phase 1: non-throwing VulkanBuffer::create() returns false and leaves the
// buffer invalid instead of throwing (Phase 0 recorded this as EXPECT_THROW).
TEST_F(VulkanBufferTest, CreateWithExcessiveSizeFails) {
    VulkanBuffer buf;
    constexpr VkDeviceSize hugeSize = static_cast<VkDeviceSize>(1) << 40; // 1 TB
    EXPECT_FALSE(buf.create(ctx_, pool_, hugeSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
    EXPECT_FALSE(buf.isValid());
}
