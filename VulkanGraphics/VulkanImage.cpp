#include "VulkanImage.h"
#include "VulkanContext.h"
#include "detail/VkCheckInternal.h"

namespace Phantom::VKG {

VkImageView VulkanImage::createView(VkDevice device, VkImage image,
                                     VkFormat format, VkImageAspectFlags aspect)
{
    VkImageViewCreateInfo ci{};
    ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image                           = image;
    ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ci.format                          = format;
    ci.subresourceRange.aspectMask     = aspect;
    ci.subresourceRange.baseMipLevel   = 0;
    ci.subresourceRange.levelCount     = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    VKG_CHECK(vkCreateImageView(device, &ci, nullptr, &view),
              "Failed to create image view", VK_NULL_HANDLE);
    return view;
}

bool VulkanImage::create(const VulkanContext& ctx,
                          uint32_t width, uint32_t height,
                          VkFormat format, VkImageTiling tiling,
                          VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                          VkImage& image, VkDeviceMemory& memory)
{
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.extent        = {width, height, 1};
    ci.mipLevels     = 1;
    ci.arrayLayers   = 1;
    ci.format        = format;
    ci.tiling        = tiling;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.usage         = usage;
    ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;

    VKG_CHECK(vkCreateImage(ctx.getDevice(), &ci, nullptr, &image),
              "Failed to create image", false);

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx.getDevice(), image, &req);

    auto memType = ctx.findMemoryType(req.memoryTypeBits, props);
    if (!memType) {
        vkDestroyImage(ctx.getDevice(), image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = *memType;

    if (vkAllocateMemory(ctx.getDevice(), &ai, nullptr, &memory) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to allocate image memory\n");
        vkDestroyImage(ctx.getDevice(), image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(ctx.getDevice(), image, memory, 0);
    return true;
}

} // namespace VKG
