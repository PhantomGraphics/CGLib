#include "VulkanTestFixture.h"
#include "../VulkanImage.h"

using Phantom::VKG::VulkanImage;

using VulkanImageTest = VulkanTestFixture;

TEST_F(VulkanImageTest, CreateAndCreateView) {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    EXPECT_TRUE(VulkanImage::create(ctx_, 4, 4, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        image, memory));

    EXPECT_NE(image, VK_NULL_HANDLE);
    EXPECT_NE(memory, VK_NULL_HANDLE);

    VkImageView view = VulkanImage::createView(ctx_.getDevice(), image,
                                               VK_FORMAT_R8G8B8A8_UNORM,
                                               VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_NE(view, VK_NULL_HANDLE);

    vkDestroyImageView(ctx_.getDevice(), view, nullptr);
    vkDestroyImage(ctx_.getDevice(), image, nullptr);
    vkFreeMemory(ctx_.getDevice(), memory, nullptr);
}

namespace {
void destroyUploaded(Phantom::VKG::VulkanContext& ctx, VkImage image, VkDeviceMemory memory, VkImageView view) {
    vkDestroyImageView(ctx.getDevice(), view, nullptr);
    vkDestroyImage(ctx.getDevice(), image, nullptr);
    vkFreeMemory(ctx.getDevice(), memory, nullptr);
}
} // namespace

TEST_F(VulkanImageTest, CreateFromPixelsRGBA8SingleLevel) {
    const uint8_t white[4] = {255, 255, 255, 255};
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t mips = 0;
    ASSERT_TRUE(VulkanImage::createFromPixelsRGBA8(ctx_, pool_, white, 1, 1, false, image, memory, view, &mips));
    EXPECT_NE(image, VK_NULL_HANDLE);
    EXPECT_NE(memory, VK_NULL_HANDLE);
    EXPECT_NE(view, VK_NULL_HANDLE);
    EXPECT_EQ(mips, 1u);
    destroyUploaded(ctx_, image, memory, view);
}

TEST_F(VulkanImageTest, CreateFromPixelsRGBA8BuildsMipChain) {
    std::vector<uint8_t> pixels(8u * 4u * 4u, 128);
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    uint32_t mips = 0;
    ASSERT_TRUE(VulkanImage::createFromPixelsRGBA8(ctx_, pool_, pixels.data(), 8, 4, true, image, memory, view, &mips));
    // 8x4 -> 8,4,2,1 = 4 levels, unless the device cannot linearly blit RGBA8 (then 1).
    EXPECT_TRUE(mips == 4u || mips == 1u) << "mips=" << mips;
    destroyUploaded(ctx_, image, memory, view);
}

TEST_F(VulkanImageTest, CreateFromPixelsRGBA8RejectsBadInputAndLeavesNullHandles) {
    const uint8_t px[4] = {1, 2, 3, 4};
    VkImage image = reinterpret_cast<VkImage>(uintptr_t(1)); // must be reset, not trusted
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    EXPECT_FALSE(VulkanImage::createFromPixelsRGBA8(ctx_, pool_, nullptr, 1, 1, false, image, memory, view));
    EXPECT_EQ(image, VK_NULL_HANDLE);
    EXPECT_FALSE(VulkanImage::createFromPixelsRGBA8(ctx_, pool_, px, 0, 1, false, image, memory, view));
    EXPECT_EQ(view, VK_NULL_HANDLE);
}
