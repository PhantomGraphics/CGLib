#include "VulkanCubeMap.h"
#include "VulkanContext.h"
#include "VulkanCommandPool.h"
#include "detail/VkCheckInternal.h"

// Define STB_IMAGE_IMPLEMENTATION here to provide stbi_load for all link units.
// If GltfReader.cpp or similar is included in the same link unit, remove its
// STB_IMAGE_IMPLEMENTATION definition to avoid duplicate symbol errors.
#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/stb/stb_image.h"

#include <cstring>

namespace Phantom::VKG {

// ---------------------------------------------------------------------------
// Internal helpers: image / view / sampler creation
// ---------------------------------------------------------------------------

bool VulkanCubeMap::createImage(const VulkanContext& ctx, uint32_t size)
{
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ci.extent        = { size, size, 1 };
    ci.mipLevels     = 1;
    ci.arrayLayers   = 6;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VKG_CHECK(vkCreateImage(ctx.getDevice(), &ci, nullptr, &image_),
              "VulkanCubeMap: image creation failed", false);

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ctx.getDevice(), image_, &req);

    auto memType = ctx.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memType) {
        vkDestroyImage(ctx.getDevice(), image_, nullptr);
        image_ = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = *memType;

    if (vkAllocateMemory(ctx.getDevice(), &ai, nullptr, &memory_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap: memory allocation failed\n");
        vkDestroyImage(ctx.getDevice(), image_, nullptr);
        image_ = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(ctx.getDevice(), image_, memory_, 0);
    return true;
}

bool VulkanCubeMap::createViewAndSampler(VkDevice device)
{
    VkImageViewCreateInfo viewCI{};
    viewCI.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image    = image_;
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCI.format   = VK_FORMAT_R8G8B8A8_UNORM;
    viewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };

    VKG_CHECK(vkCreateImageView(device, &viewCI, nullptr, &imageView_),
              "VulkanCubeMap: image view creation failed", false);

    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter    = VK_FILTER_LINEAR;
    samplerCI.minFilter    = VK_FILTER_LINEAR;
    samplerCI.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.maxLod       = 0.0f;

    if (vkCreateSampler(device, &samplerCI, nullptr, &sampler_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap: sampler creation failed\n");
        vkDestroyImageView(device, imageView_, nullptr);
        imageView_ = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Layout transition helper
// ---------------------------------------------------------------------------

static void transitionLayout(VkCommandBuffer cmd, VkImage image,
                              VkImageLayout oldLayout, VkImageLayout newLayout,
                              VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                              VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    barrier.srcAccessMask       = srcAccess;
    barrier.dstAccessMask       = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// ---------------------------------------------------------------------------
// create: load 6 faces with stb_image
// ---------------------------------------------------------------------------

bool VulkanCubeMap::create(const VulkanContext& ctx,
                            const VulkanCommandPool& pool,
                            const std::array<std::string, 6>& facePaths)
{
    VkDevice device = ctx.getDevice();

    // Load 6 faces
    int faceW = 0, faceH = 0;
    std::array<stbi_uc*, 6> faceData{};

    for (int i = 0; i < 6; ++i) {
        int w, h, ch;
        faceData[i] = stbi_load(facePaths[i].c_str(), &w, &h, &ch, STBI_rgb_alpha);
        if (!faceData[i]) {
            for (int j = 0; j < i; ++j) stbi_image_free(faceData[j]);
            std::fprintf(stderr, "[VKG] VulkanCubeMap: failed to load %s\n", facePaths[i].c_str());
            return false;
        }
        if (i == 0) { faceW = w; faceH = h; }
    }

    const VkDeviceSize faceBytes = static_cast<VkDeviceSize>(faceW) * faceH * 4;
    const VkDeviceSize totalBytes = faceBytes * 6;

    // Create staging buffer
    VkBufferCreateInfo stagingBCI{};
    stagingBCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBCI.size        = totalBytes;
    stagingBCI.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    if (vkCreateBuffer(device, &stagingBCI, nullptr, &stagingBuf) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap: staging buffer failed\n");
        return false;
    }

    VkMemoryRequirements stagingReq;
    vkGetBufferMemoryRequirements(device, stagingBuf, &stagingReq);

    auto memType = ctx.findMemoryType(stagingReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!memType) {
        vkDestroyBuffer(device, stagingBuf, nullptr);
        return false;
    }

    VkMemoryAllocateInfo stagingMAI{};
    stagingMAI.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingMAI.allocationSize  = stagingReq.size;
    stagingMAI.memoryTypeIndex = *memType;

    if (vkAllocateMemory(device, &stagingMAI, nullptr, &stagingMem) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap: staging memory failed\n");
        vkDestroyBuffer(device, stagingBuf, nullptr);
        return false;
    }
    vkBindBufferMemory(device, stagingBuf, stagingMem, 0);

    void* mapped = nullptr;
    vkMapMemory(device, stagingMem, 0, totalBytes, 0, &mapped);
    for (int i = 0; i < 6; ++i) {
        std::memcpy(static_cast<char*>(mapped) + faceBytes * i, faceData[i], static_cast<size_t>(faceBytes));
        stbi_image_free(faceData[i]);
    }
    vkUnmapMemory(device, stagingMem);

    // Create cube image
    if (!createImage(ctx, static_cast<uint32_t>(faceW))) {
        vkDestroyBuffer(device, stagingBuf, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
        return false;
    }

    // Upload via command buffer
    VkCommandBuffer cmd = pool.beginSingleTimeCommands();

    transitionLayout(cmd, image_,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    for (uint32_t i = 0; i < 6; ++i) {
        VkBufferImageCopy region{};
        region.bufferOffset      = faceBytes * i;
        region.imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, i, 1 };
        region.imageExtent       = { static_cast<uint32_t>(faceW), static_cast<uint32_t>(faceH), 1 };
        vkCmdCopyBufferToImage(cmd, stagingBuf, image_,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    transitionLayout(cmd, image_,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    pool.endSingleTimeCommands(cmd);

    vkDestroyBuffer(device, stagingBuf, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    return createViewAndSampler(device);
}

// ---------------------------------------------------------------------------
// createDummy: 1x1 black cubemap (no stb_image required)
// ---------------------------------------------------------------------------

bool VulkanCubeMap::createDummy(const VulkanContext& ctx, const VulkanCommandPool& pool)
{
    VkDevice device = ctx.getDevice();

    // 1x1 black RGBA pixel repeated for each of the 6 faces
    const uint8_t kBlack[4] = { 0, 0, 0, 255 };
    constexpr VkDeviceSize faceBytes  = 4;
    constexpr VkDeviceSize totalBytes = 4 * 6;

    VkBufferCreateInfo stagingBCI{};
    stagingBCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBCI.size        = totalBytes;
    stagingBCI.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;

    if (vkCreateBuffer(device, &stagingBCI, nullptr, &stagingBuf) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap::createDummy: staging buffer failed\n");
        return false;
    }

    VkMemoryRequirements stagingReq;
    vkGetBufferMemoryRequirements(device, stagingBuf, &stagingReq);

    auto memType = ctx.findMemoryType(stagingReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!memType) {
        vkDestroyBuffer(device, stagingBuf, nullptr);
        return false;
    }

    VkMemoryAllocateInfo stagingMAI{};
    stagingMAI.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingMAI.allocationSize  = stagingReq.size;
    stagingMAI.memoryTypeIndex = *memType;

    if (vkAllocateMemory(device, &stagingMAI, nullptr, &stagingMem) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap::createDummy: staging memory failed\n");
        vkDestroyBuffer(device, stagingBuf, nullptr);
        return false;
    }
    vkBindBufferMemory(device, stagingBuf, stagingMem, 0);

    void* mapped = nullptr;
    vkMapMemory(device, stagingMem, 0, totalBytes, 0, &mapped);
    for (int i = 0; i < 6; ++i)
        std::memcpy(static_cast<char*>(mapped) + faceBytes * i, kBlack, faceBytes);
    vkUnmapMemory(device, stagingMem);

    if (!createImage(ctx, 1)) {
        vkDestroyBuffer(device, stagingBuf, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
        return false;
    }

    VkCommandBuffer cmd = pool.beginSingleTimeCommands();

    transitionLayout(cmd, image_,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    for (uint32_t i = 0; i < 6; ++i) {
        VkBufferImageCopy region{};
        region.bufferOffset     = faceBytes * i;
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, i, 1 };
        region.imageExtent      = { 1, 1, 1 };
        vkCmdCopyBufferToImage(cmd, stagingBuf, image_,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    transitionLayout(cmd, image_,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    pool.endSingleTimeCommands(cmd);

    vkDestroyBuffer(device, stagingBuf, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    return createViewAndSampler(device);
}

// ---------------------------------------------------------------------------
// destroy
// ---------------------------------------------------------------------------

void VulkanCubeMap::destroy(VkDevice device)
{
    if (sampler_)   { vkDestroySampler(device, sampler_, nullptr);     sampler_   = VK_NULL_HANDLE; }
    if (imageView_) { vkDestroyImageView(device, imageView_, nullptr); imageView_ = VK_NULL_HANDLE; }
    if (image_)     { vkDestroyImage(device, image_, nullptr);         image_     = VK_NULL_HANDLE; }
    if (memory_)    { vkFreeMemory(device, memory_, nullptr);          memory_    = VK_NULL_HANDLE; }
}

} // namespace VKG
