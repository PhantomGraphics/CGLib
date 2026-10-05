#include "VulkanImage.h"
#include "VulkanContext.h"
#include "VulkanCommandPool.h"
#include "VulkanBuffer.h"
#include "detail/VkCheckInternal.h"

#include <algorithm>

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

bool VulkanImage::createFromPixelsRGBA8(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                        const uint8_t* pixels, uint32_t width, uint32_t height,
                                        bool generateMips,
                                        VkImage& image, VkDeviceMemory& memory, VkImageView& view,
                                        uint32_t* outMipLevels)
{
    return createFromPixels(ctx, pool, pixels, 4, width, height, VK_FORMAT_R8G8B8A8_UNORM, generateMips,
                            image, memory, view, outMipLevels);
}

bool VulkanImage::createFromPixels(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                   const uint8_t* pixels, uint32_t bytesPerPixel,
                                   uint32_t width, uint32_t height, VkFormat format,
                                   bool generateMips,
                                   VkImage& image, VkDeviceMemory& memory, VkImageView& view,
                                   uint32_t* outMipLevels)
{
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    view = VK_NULL_HANDLE;
    if (outMipLevels) *outMipLevels = 1;
    if (!pixels || width == 0 || height == 0 || bytesPerPixel == 0) return false;

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * bytesPerPixel;

    uint32_t mipLevels = 1;
    if (generateMips) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(ctx.getPhysicalDevice(), format, &props);
        const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if ((props.optimalTilingFeatures & need) == need) {
            for (uint32_t size = std::max(width, height); size > 1; size >>= 1)
                ++mipLevels;
        }
    }

    VulkanBuffer staging;
    if (!staging.createMapped(ctx, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) return false;
    staging.write(pixels, imageSize);

    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (mipLevels > 1) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (!create(ctx, width, height, format, VK_IMAGE_TILING_OPTIMAL, usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory, mipLevels)) {
        staging.destroy();
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }

    VkCommandBuffer cmd = pool.beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent      = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, staging.get(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Each level is blitted from the previous one, which is first moved to TRANSFER_SRC and,
    // once consumed, to SHADER_READ_ONLY; the last level goes straight to SHADER_READ_ONLY.
    int32_t mipW = static_cast<int32_t>(width), mipH = static_cast<int32_t>(height);
    for (uint32_t level = 1; level < mipLevels; ++level) {
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1};
        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        const int32_t nextW = std::max(mipW / 2, 1);
        const int32_t nextH = std::max(mipH / 2, 1);
        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1};
        blit.srcOffsets[1]  = {mipW, mipH, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
        blit.dstOffsets[1]  = {nextW, nextH, 1};
        vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        mipW = nextW;
        mipH = nextH;
    }

    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1, 0, 1};
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    pool.endSingleTimeCommands(cmd);
    staging.destroy();

    view = createView(ctx.getDevice(), image, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
    if (view == VK_NULL_HANDLE) {
        vkDestroyImage(ctx.getDevice(), image, nullptr);
        vkFreeMemory(ctx.getDevice(), memory, nullptr);
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }
    if (outMipLevels) *outMipLevels = mipLevels;
    return true;
}

bool VulkanImage::createCubeFromFacesRGBA8(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                           const uint8_t* faces, uint32_t size,
                                           VkImage& image, VkDeviceMemory& memory, VkImageView& view)
{
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    view = VK_NULL_HANDLE;
    if (!faces || size == 0) return false;

    const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    const VkDeviceSize faceBytes = static_cast<VkDeviceSize>(size) * size * 4;
    const VkDeviceSize totalBytes = faceBytes * 6;
    VkDevice dev = ctx.getDevice();

    VulkanBuffer staging;
    if (!staging.createMapped(ctx, totalBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) return false;
    staging.write(faces, totalBytes);

    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.format        = format;
    ci.extent        = {size, size, 1};
    ci.mipLevels     = 1;
    ci.arrayLayers   = 6;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    auto fail = [&]() {
        if (image)  vkDestroyImage(dev, image, nullptr);
        if (memory) vkFreeMemory(dev, memory, nullptr);
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        view = VK_NULL_HANDLE;
        staging.destroy();
        return false;
    };
    if (vkCreateImage(dev, &ci, nullptr, &image) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to create cube image\n");
        image = VK_NULL_HANDLE;
        return fail();
    }
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(dev, image, &req);
    auto memType = ctx.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memType) return fail();
    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = *memType;
    if (vkAllocateMemory(dev, &ai, nullptr, &memory) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to allocate cube image memory\n");
        memory = VK_NULL_HANDLE;
        return fail();
    }
    if (vkBindImageMemory(dev, image, memory, 0) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to bind cube image memory\n");
        return fail();
    }

    VkCommandBuffer cmd = pool.beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copies[6]{};
    for (uint32_t f = 0; f < 6; ++f) {
        copies[f].bufferOffset                    = faceBytes * f;
        copies[f].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copies[f].imageSubresource.layerCount     = 1;
        copies[f].imageSubresource.baseArrayLayer = f;
        copies[f].imageExtent                     = {size, size, 1};
    }
    vkCmdCopyBufferToImage(cmd, staging.get(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, copies);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    pool.endSingleTimeCommands(cmd);
    staging.destroy();

    VkImageViewCreateInfo vci{};
    vci.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image            = image;
    vci.viewType         = VK_IMAGE_VIEW_TYPE_CUBE;
    vci.format           = format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
    if (vkCreateImageView(dev, &vci, nullptr, &view) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to create cube image view\n");
        view = VK_NULL_HANDLE;
        return fail();
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
