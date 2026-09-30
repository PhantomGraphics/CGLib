#include "Volume3DImage.h"
#include "VolumeGpuUtil.h"

#include "../../VulkanGraphics/VulkanCommandPool.h"
#include "../../VulkanGraphics/VulkanContext.h"

#include <cstdio>
#include <cstring>

namespace Phantom::Volume {

namespace {
constexpr VkFormat kFormat = VK_FORMAT_R32_SFLOAT;

using detail::HostBuffer;
inline void transition(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                       VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage,
                       VkPipelineStageFlags dstStage)
{
    detail::imageBarrier(cmd, image, from, to, srcAccess, dstAccess, srcStage, dstStage);
}
}

bool Volume3DImage::isSupported(const Phantom::VKG::VulkanContext& ctx)
{
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(ctx.getPhysicalDevice(), kFormat, &props);
    const VkFormatFeatureFlags need = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                      VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                      VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    return (props.optimalTilingFeatures & need) == need;
}

bool Volume3DImage::create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                           uint32_t nx, uint32_t ny, uint32_t nz)
{
    if (nx == 0 || ny == 0 || nz == 0 || !isSupported(ctx)) {
        std::fprintf(stderr, "[Volume] Volume3DImage: bad size or R32F 3D format unsupported\n");
        return false;
    }
    VkDevice device = ctx.getDevice();
    nx_ = nx; ny_ = ny; nz_ = nz;

    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_3D;
    ci.format = kFormat;
    ci.extent = { nx, ny, nz };
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &ci, nullptr, &image_) != VK_SUCCESS) return false;

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, image_, &req);
    auto type = ctx.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!type) { destroy(ctx); return false; }
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = *type;
    if (vkAllocateMemory(device, &ai, nullptr, &memory_) != VK_SUCCESS) { destroy(ctx); return false; }
    vkBindImageMemory(device, image_, memory_, 0);

    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = image_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_3D;
    vi.format = kFormat;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device, &vi, nullptr, &view_) != VK_SUCCESS) { destroy(ctx); return false; }

    // UNDEFINED -> GENERAL (stays GENERAL for its whole life), cleared to zero.
    VkCommandBuffer cmd = pool.beginSingleTimeCommands();
    transition(cmd, image_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkClearColorValue zero{};
    VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdClearColorImage(cmd, image_, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
    transition(cmd, image_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    pool.endSingleTimeCommands(cmd);
    return true;
}

void Volume3DImage::destroy(const Phantom::VKG::VulkanContext& ctx)
{
    VkDevice device = ctx.getDevice();
    if (view_) vkDestroyImageView(device, view_, nullptr);
    if (image_) vkDestroyImage(device, image_, nullptr);
    if (memory_) vkFreeMemory(device, memory_, nullptr);
    view_ = VK_NULL_HANDLE;
    image_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
    nx_ = ny_ = nz_ = 0;
}

bool Volume3DImage::upload(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                           const float* data, size_t count) const
{
    const size_t expected = static_cast<size_t>(nx_) * ny_ * nz_;
    if (!isValid() || count != expected) return false;
    HostBuffer staging;
    if (!staging.create(ctx, expected * sizeof(float), VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) {
        staging.destroy(ctx);
        return false;
    }
    std::memcpy(staging.mapped, data, expected * sizeof(float));

    VkCommandBuffer cmd = pool.beginSingleTimeCommands();
    transition(cmd, image_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { nx_, ny_, nz_ };
    vkCmdCopyBufferToImage(cmd, staging.buffer, image_, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    transition(cmd, image_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    pool.endSingleTimeCommands(cmd);
    staging.destroy(ctx);
    return true;
}

bool Volume3DImage::download(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                             std::vector<float>& out) const
{
    if (!isValid()) return false;
    const size_t count = static_cast<size_t>(nx_) * ny_ * nz_;
    HostBuffer staging;
    if (!staging.create(ctx, count * sizeof(float), VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
        staging.destroy(ctx);
        return false;
    }
    VkCommandBuffer cmd = pool.beginSingleTimeCommands();
    transition(cmd, image_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
               VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { nx_, ny_, nz_ };
    vkCmdCopyImageToBuffer(cmd, image_, VK_IMAGE_LAYOUT_GENERAL, staging.buffer, 1, &region);
    pool.endSingleTimeCommands(cmd);
    out.assign(static_cast<const float*>(staging.mapped), static_cast<const float*>(staging.mapped) + count);
    staging.destroy(ctx);
    return true;
}

void Volume3DImage::recordWriteToRead(VkCommandBuffer cmd) const
{
    transition(cmd, image_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

} // namespace Phantom::Volume
