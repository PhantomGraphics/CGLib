#include "VulkanTextureHelper.h"

#include "CGLib/VulkanGraphics/VulkanContext.h"
#include "CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "CGLib/VulkanGraphics/VulkanImage.h"

// stb_image implementation is provided by VulkanGraphics.lib (VulkanCubeMap.cpp).
#include <stb_image.h>

#include <cstdio>

namespace Phantom::Animation {

GpuTexture VulkanTextureHelper::uploadPixels(
    const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
    const uint8_t* pixels, int w, int h)
{
    GpuTexture t;
    // Single level (no mip chain): the sampler below keeps the default LOD range.
    if (!::VKG::VulkanImage::createFromPixelsRGBA8(ctx, pool, pixels,
            static_cast<uint32_t>(w), static_cast<uint32_t>(h), false,
            t.image, t.memory, t.view))
        return GpuTexture{};

    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    if (vkCreateSampler(ctx.getDevice(), &sci, nullptr, &t.sampler) != VK_SUCCESS) {
        vkDestroyImageView(ctx.getDevice(), t.view, nullptr);
        vkDestroyImage(ctx.getDevice(), t.image, nullptr);
        vkFreeMemory(ctx.getDevice(), t.memory, nullptr);
        return GpuTexture{};
    }
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

    // A path that already failed is not retried (it would re-read the file and log every call).
    if (failed_.count(absPath)) return fallback_;

    int w = 0, h = 0, ch = 0;
    uint8_t* pixels = stbi_load(absPath.c_str(), &w, &h, &ch, 4);
    if (!pixels || w <= 0 || h <= 0) {
        std::fprintf(stderr, "[VulkanTextureHelper] Failed to load: %s\n", absPath.c_str());
        if (pixels) stbi_image_free(pixels);
        failed_.insert(absPath);
        return fallback_;
    }

    GpuTexture t = uploadPixels(ctx, pool, pixels, w, h);
    stbi_image_free(pixels);

    if (t.image == VK_NULL_HANDLE) {
        failed_.insert(absPath);
        return fallback_;
    }

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
    failed_.clear();

    if (fallback_.sampler) vkDestroySampler(device, fallback_.sampler, nullptr);
    if (fallback_.view)    vkDestroyImageView(device, fallback_.view, nullptr);
    if (fallback_.image)   vkDestroyImage(device, fallback_.image, nullptr);
    if (fallback_.memory)  vkFreeMemory(device, fallback_.memory, nullptr);
    fallback_ = GpuTexture{};
}

} // namespace Phantom::Animation
