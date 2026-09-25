#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanCommandPool.h"
#include "detail/VkCheckInternal.h"

namespace Phantom::VKG {

VkImageView VulkanImage::createView(VkDevice device, VkImage image,
                                     VkFormat format, VkImageAspectFlags aspect,
                                     uint32_t mipLevels)
{
    VkImageViewCreateInfo ci{};
    ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image                           = image;
    ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    ci.format                          = format;
    ci.subresourceRange.aspectMask     = aspect;
    ci.subresourceRange.baseMipLevel   = 0;
    ci.subresourceRange.levelCount     = mipLevels;
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
                          VkImage& image, VkDeviceMemory& memory,
                          uint32_t mipLevels)
{
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.extent        = {width, height, 1};
    ci.mipLevels     = mipLevels;
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

    if (vkBindImageMemory(ctx.getDevice(), image, memory, 0) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to bind image memory\n");
        vkFreeMemory(ctx.getDevice(), memory, nullptr);
        vkDestroyImage(ctx.getDevice(), image, nullptr);
        memory = VK_NULL_HANDLE;
        image  = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

bool VulkanImage::createZeroArrayTexture(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                         VkFormat format, VkImage& image, VkDeviceMemory& memory,
                                         VkImageView& view)
{
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    view = VK_NULL_HANDLE;
    if (!create(ctx, 1, 1, format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory))
        return false;

    VkCommandBuffer cmd = pool.beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    const VkClearColorValue zero{};
    const VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &range);
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    pool.endSingleTimeCommands(cmd);

    VkImageViewCreateInfo vi{};
    vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image            = image;
    vi.viewType         = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    vi.format           = format;
    vi.subresourceRange = range;
    if (vkCreateImageView(ctx.getDevice(), &vi, nullptr, &view) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to create zero array texture view\n");
        vkDestroyImage(ctx.getDevice(), image, nullptr);
        vkFreeMemory(ctx.getDevice(), memory, nullptr);
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        view = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

} // namespace VKG
