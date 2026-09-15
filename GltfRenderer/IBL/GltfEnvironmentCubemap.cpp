#include "GltfEnvironmentCubemap.h"
#include "GltfIBLPrecomputer.h"
#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanImage.h"
#include "../../../CGLib/Graphics/ImageFileReader.h"

#include <glm/gtc/packing.hpp>
#include <cstdio>
#include <cstring>

using namespace Phantom::Gltf;

namespace {
constexpr VkFormat   kFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr uint32_t   kSize   = 1;
constexpr uint32_t   kFaces  = 6;
// Dim sky tint, not black: see GltfEnvironmentCubemap.h's comment for why (a bright flat
// environment washes out every material's own texture detail once IBL is enabled).
constexpr float kSkyColor[4] = { 0.05f, 0.07f, 0.10f, 1.0f };
} // namespace

bool GltfEnvironmentCubemap::create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool)
{
    VkDevice device = ctx.getDevice();
    const VkDeviceSize pixBytes = kSize * kSize * 4 * sizeof(float);
    const VkDeviceSize total    = pixBytes * kFaces;

    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = kFormat;
    ici.extent        = { kSize, kSize, 1 };
    ici.mipLevels     = 1;
    ici.arrayLayers   = kFaces;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &ici, nullptr, &image_) != VK_SUCCESS) return false;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device, image_, &mr);
    auto memType = ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memType) { destroy(device); return false; }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = mr.size;
    mai.memoryTypeIndex = *memType;
    if (vkAllocateMemory(device, &mai, nullptr, &memory_) != VK_SUCCESS) { destroy(device); return false; }
    vkBindImageMemory(device, image_, memory_, 0);

    // Staging buffer: the same dim tint repeated for all 6 faces.
    VkBuffer stageBuf = VK_NULL_HANDLE;
    VkDeviceMemory stageMem = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bci{};
        bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size        = total;
        bci.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bci, nullptr, &stageBuf) != VK_SUCCESS) { destroy(device); return false; }

        VkMemoryRequirements smr;
        vkGetBufferMemoryRequirements(device, stageBuf, &smr);
        auto stageMemType = ctx.findMemoryType(smr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!stageMemType) { vkDestroyBuffer(device, stageBuf, nullptr); destroy(device); return false; }

        VkMemoryAllocateInfo smai{};
        smai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        smai.allocationSize  = smr.size;
        smai.memoryTypeIndex = *stageMemType;
        if (vkAllocateMemory(device, &smai, nullptr, &stageMem) != VK_SUCCESS) {
            vkDestroyBuffer(device, stageBuf, nullptr);
            destroy(device);
            return false;
        }
        vkBindBufferMemory(device, stageBuf, stageMem, 0);

        void* mapped = nullptr;
        vkMapMemory(device, stageMem, 0, total, 0, &mapped);
        for (uint32_t f = 0; f < kFaces; ++f)
            std::memcpy(static_cast<char*>(mapped) + pixBytes * f, kSkyColor, sizeof(kSkyColor));
        vkUnmapMemory(device, stageMem);
    }

    // UNDEFINED -> TRANSFER_DST -> SHADER_READ_ONLY
    {
        VkCommandBuffer cmd = pool.beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image_;
        barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, kFaces };
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        for (uint32_t f = 0; f < kFaces; ++f) {
            VkBufferImageCopy region{};
            region.bufferOffset                    = pixBytes * f;
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.baseArrayLayer = f;
            region.imageSubresource.layerCount     = 1;
            region.imageExtent                     = { kSize, kSize, 1 };
            vkCmdCopyBufferToImage(cmd, stageBuf, image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        pool.endSingleTimeCommands(cmd);
    }

    vkDestroyBuffer(device, stageBuf, nullptr);
    vkFreeMemory(device, stageMem, nullptr);

    VkImageViewCreateInfo vci{};
    vci.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image            = image_;
    vci.viewType         = VK_IMAGE_VIEW_TYPE_CUBE;
    vci.format           = kFormat;
    vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, kFaces };
    if (vkCreateImageView(device, &vci, nullptr, &view_) != VK_SUCCESS) { destroy(device); return false; }

    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod       = 1.0f;
    if (vkCreateSampler(device, &sci, nullptr, &sampler_) != VK_SUCCESS) { destroy(device); return false; }

    return true;
}

void GltfEnvironmentCubemap::destroy(VkDevice device)
{
    if (sampler_) { vkDestroySampler(device, sampler_, nullptr);   sampler_ = VK_NULL_HANDLE; }
    if (view_)    { vkDestroyImageView(device, view_, nullptr);    view_    = VK_NULL_HANDLE; }
    if (image_)   { vkDestroyImage(device, image_, nullptr);       image_   = VK_NULL_HANDLE; }
    if (memory_)  { vkFreeMemory(device, memory_, nullptr);        memory_  = VK_NULL_HANDLE; }
    isRealHDR_ = false;
    hdrPath_.clear();
}

namespace {

// Loads an equirectangular .hdr panorama and uploads it as a 2D VK_FORMAT_R16G16B16A16_SFLOAT
// texture (half float, not the .hdr file's native float32 -- matches the format
// GltfIBLPrecomputer already uses for the cube it bakes from this, and R16G16B16A16_SFLOAT's
// sampled-image-with-linear-filtering support is part of Vulkan's core mandatory format list,
// unlike the 128-bit R32G32B32A32_SFLOAT this would otherwise need to preserve full precision).
// `outImage`/`outMem`/`outView`/`outSampler` may be partially populated even on failure (e.g. the
// image was created but the view failed) -- the caller destroys whichever handles are non-null
// either way. On success the caller owns their lifetime and must destroy them once done with the
// temporary equirect texture (it is only needed for the duration of computeEnvironmentCube()'s
// render pass).
bool createEquirectTexture(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                           const std::string& path,
                           VkImage& outImage, VkDeviceMemory& outMem,
                           VkImageView& outView, VkSampler& outSampler)
{
    Phantom::Graphics::HDRImageFileReader reader;
    if (!reader.read(path)) {
        std::fprintf(stderr, "[GltfEnvironmentCubemap] failed to read HDR file: %s\n", path.c_str());
        return false;
    }
    Phantom::Graphics::Imagef img = reader.toImage();
    const int w = img.getWidth();
    const int h = img.getHeight();
    if (w <= 0 || h <= 0) return false;

    const std::vector<float> px = img.getValues(); // flat RGBA, w*h*4 floats (returned by value)
    std::vector<glm::u16vec4> half(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (size_t i = 0; i < half.size(); ++i) {
        const glm::vec4 c(px[i * 4 + 0], px[i * 4 + 1], px[i * 4 + 2], px[i * 4 + 3]);
        half[i] = glm::packHalf(c);
    }

    constexpr VkFormat fmt = VK_FORMAT_R16G16B16A16_SFLOAT;
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(half.size()) * sizeof(glm::u16vec4);
    VkDevice device = ctx.getDevice();

    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = imageSize;
        bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bi, nullptr, &stagingBuf) != VK_SUCCESS) return false;

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(device, stagingBuf, &mr);
        auto memType = ctx.findMemoryType(mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!memType) { vkDestroyBuffer(device, stagingBuf, nullptr); return false; }

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = *memType;
        if (vkAllocateMemory(device, &ai, nullptr, &stagingMem) != VK_SUCCESS) {
            vkDestroyBuffer(device, stagingBuf, nullptr);
            return false;
        }
        vkBindBufferMemory(device, stagingBuf, stagingMem, 0);

        void* mapped = nullptr;
        vkMapMemory(device, stagingMem, 0, imageSize, 0, &mapped);
        std::memcpy(mapped, half.data(), static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingMem);
    }

    if (!Phantom::VKG::VulkanImage::create(ctx, static_cast<uint32_t>(w), static_cast<uint32_t>(h), fmt,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            outImage, outMem)) {
        vkDestroyBuffer(device, stagingBuf, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
        return false;
    }

    {
        VkCommandBuffer cmd = pool.beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = outImage;
        barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent      = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 };
        vkCmdCopyBufferToImage(cmd, stagingBuf, outImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        pool.endSingleTimeCommands(cmd);
    }

    vkDestroyBuffer(device, stagingBuf, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    outView = Phantom::VKG::VulkanImage::createView(device, outImage, fmt, VK_IMAGE_ASPECT_COLOR_BIT);
    if (outView == VK_NULL_HANDLE) return false;

    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;   // wraps around the horizon seam
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod       = 1.0f;
    if (vkCreateSampler(device, &sci, nullptr, &outSampler) != VK_SUCCESS) return false;

    return true;
}

} // namespace

bool GltfEnvironmentCubemap::loadFromHDR(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                         const std::string& path,
                                         std::vector<uint32_t> equirectVert, std::vector<uint32_t> equirectFrag,
                                         uint32_t cubeSize)
{
    VkDevice device = ctx.getDevice();

    VkImage        eqImage   = VK_NULL_HANDLE;
    VkDeviceMemory eqMem     = VK_NULL_HANDLE;
    VkImageView    eqView    = VK_NULL_HANDLE;
    VkSampler      eqSampler = VK_NULL_HANDLE;
    if (!createEquirectTexture(ctx, pool, path, eqImage, eqMem, eqView, eqSampler)) {
        if (eqSampler) vkDestroySampler(device, eqSampler, nullptr);
        if (eqView)    vkDestroyImageView(device, eqView, nullptr);
        if (eqImage)   vkDestroyImage(device, eqImage, nullptr);
        if (eqMem)     vkFreeMemory(device, eqMem, nullptr);
        return false;
    }

    // Fresh instance per call: GltfIBLPrecomputer holds no state beyond a single compute()/
    // computeEnvironmentCube() invocation's own cube geometry buffers (created/destroyed inside
    // the call), so there is nothing to share across loadFromHDR() calls.
    GltfIBLPrecomputer precomputer;
    auto cube = precomputer.computeEnvironmentCube(ctx, pool, eqView, eqSampler, cubeSize,
                                                    std::move(equirectVert), std::move(equirectFrag));

    vkDestroySampler(device, eqSampler, nullptr);
    vkDestroyImageView(device, eqView, nullptr);
    vkDestroyImage(device, eqImage, nullptr);
    vkFreeMemory(device, eqMem, nullptr);

    if (!cube) {
        std::fprintf(stderr, "[GltfEnvironmentCubemap] equirect-to-cube conversion failed for: %s\n", path.c_str());
        return false;
    }

    destroy(device); // drop whatever cubemap (placeholder or a previous HDRI) was active
    image_   = cube->image;
    memory_  = cube->mem;
    view_    = cube->view;
    sampler_ = cube->sampler;
    isRealHDR_ = true;
    hdrPath_   = path;
    return true;
}

bool GltfEnvironmentCubemap::resetToPlaceholder(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool)
{
    destroy(ctx.getDevice());
    return create(ctx, pool); // create() already leaves isRealHDR_/hdrPath_ at their defaults
}
