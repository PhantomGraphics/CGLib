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
