#include "VulkanTextureHelper.h"

#include "CGLib/VulkanGraphics/VulkanContext.h"
#include "CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "CGLib/VulkanGraphics/VulkanImage.h"

// stb_image implementation is provided by VulkanGraphics.lib (VulkanCubeMap.cpp).
#include <stb_image.h>

#include <cassert>
#include <cstring>
#include <cstdio>

namespace Phantom::Animation {

static void createTextureFromPixels(
    const Phantom::VKG::VulkanContext& ctx,
    const Phantom::VKG::VulkanCommandPool& pool,
    const uint8_t* pixels, int w, int h,
    VkImage& outImage, VkDeviceMemory& outMemory, VkImageView& outView)
{
    const VkDeviceSize imgSize = static_cast<VkDeviceSize>(w) * h * 4;

    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = imgSize;
        bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(ctx.getDevice(), &bi, nullptr, &stagingBuf);

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(ctx.getDevice(), stagingBuf, &mr);
        auto memType = ctx.findMemoryType(mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        assert(memType.has_value());

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = memType.value_or(0);
        vkAllocateMemory(ctx.getDevice(), &ai, nullptr, &stagingMem);
        vkBindBufferMemory(ctx.getDevice(), stagingBuf, stagingMem, 0);

        void* mapped;
        vkMapMemory(ctx.getDevice(), stagingMem, 0, imgSize, 0, &mapped);
        std::memcpy(mapped, pixels, static_cast<size_t>(imgSize));
        vkUnmapMemory(ctx.getDevice(), stagingMem);
    }

    ::VKG::VulkanImage::create(ctx, (uint32_t)w, (uint32_t)h,
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        outImage, outMemory);

    auto cmd = pool.beginSingleTimeCommands();
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = outImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent      = {(uint32_t)w, (uint32_t)h, 1};
        vkCmdCopyBufferToImage(cmd, stagingBuf, outImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    pool.endSingleTimeCommands(cmd);

    vkDestroyBuffer(ctx.getDevice(), stagingBuf, nullptr);
    vkFreeMemory(ctx.getDevice(), stagingMem, nullptr);

    outView = ::VKG::VulkanImage::createView(ctx.getDevice(), outImage,
                                            VK_FORMAT_R8G8B8A8_UNORM,
                                            VK_IMAGE_ASPECT_COLOR_BIT);
}

GpuTexture VulkanTextureHelper::uploadPixels(
    const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
    const uint8_t* pixels, int w, int h)
{
    GpuTexture t;
    createTextureFromPixels(ctx, pool, pixels, w, h, t.image, t.memory, t.view);

    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkCreateSampler(ctx.getDevice(), &sci, nullptr, &t.sampler);
    return t;
}

bool VulkanTextureHelper::createFallback(
    const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool)
{
    if (fallback_.image != VK_NULL_HANDLE) return true;
    const uint8_t white[4] = {255, 255, 255, 255};
    fallback_ = uploadPixels(ctx, pool, white, 1, 1);
    return fallback_.image != VK_NULL_HANDLE;
}

const GpuTexture& VulkanTextureHelper::load(
    const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
    const std::string& absPath)
{
    auto it = cache_.find(absPath);
    if (it != cache_.end()) return it->second;

    int w, h, ch;
    uint8_t* pixels = stbi_load(absPath.c_str(), &w, &h, &ch, 4);
    if (!pixels) {
        std::fprintf(stderr, "[VulkanTextureHelper] Failed to load: %s\n", absPath.c_str());
        return fallback_;
    }

    GpuTexture t = uploadPixels(ctx, pool, pixels, w, h);
    stbi_image_free(pixels);

    if (t.image == VK_NULL_HANDLE) return fallback_;

    cache_[absPath] = t;
    return cache_[absPath];
}

void VulkanTextureHelper::destroyAll(VkDevice device)
{
    for (auto& [path, tex] : cache_) {
        if (tex.sampler) vkDestroySampler(device, tex.sampler, nullptr);
        if (tex.view)    vkDestroyImageView(device, tex.view, nullptr);
        if (tex.image)   vkDestroyImage(device, tex.image, nullptr);
        if (tex.memory)  vkFreeMemory(device, tex.memory, nullptr);
    }
    cache_.clear();

    if (fallback_.sampler) vkDestroySampler(device, fallback_.sampler, nullptr);
    if (fallback_.view)    vkDestroyImageView(device, fallback_.view, nullptr);
    if (fallback_.image)   vkDestroyImage(device, fallback_.image, nullptr);
    if (fallback_.memory)  vkFreeMemory(device, fallback_.memory, nullptr);
    fallback_ = GpuTexture{};
}

} // namespace Phantom::Animation
