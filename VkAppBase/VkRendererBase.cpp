#include "VkRendererBase.h"

#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../CGLib/VulkanGraphics/VulkanImage.h"

#include <cassert>
#include <cstring>

namespace VKG {

// -----------------------------------------------------------------
//  onSwapChainCreated
// -----------------------------------------------------------------
void VkRendererBase::onSwapChainCreated()
{
    // Create offscreen target with the same size as the swapchain.
    const VkExtent2D ext = getExtent();
    auto depthFmtOpt = getSwapChain().findDepthFormat();
    assert(depthFmtOpt.has_value());
    const VkFormat depthFmt = depthFmtOpt.value_or(VK_FORMAT_UNDEFINED);

    offscreen_.create(getContext(), ext.width, ext.height,
                      offscreenColorFormat_, depthFmt);

    // Recreate readback buffer lazily when readOffscreenPixel() is called.
    readbackReady_ = false;
}

// -----------------------------------------------------------------
//  onSwapChainDestroying
// -----------------------------------------------------------------
void VkRendererBase::onSwapChainDestroying()
{
    offscreen_.destroy(getContext());

    if (readbackReady_) {
        readbackBuffer_.destroy(getDevice());
        readbackReady_ = false;
    }
}

// -----------------------------------------------------------------
//  onCleanup
// -----------------------------------------------------------------
void VkRendererBase::onCleanup()
{
    if (offscreen_.isValid())
        offscreen_.destroy(getContext());

    if (readbackReady_) {
        readbackBuffer_.destroy(getDevice());
        readbackReady_ = false;
    }
}

// -----------------------------------------------------------------
//  onPreRender - record offscreen pass before swapchain render pass
// -----------------------------------------------------------------
void VkRendererBase::onPreRender(VkCommandBuffer cmd, uint32_t frameIndex)
{
    offscreen_.beginRenderPass(cmd);
    onRenderOffscreen(cmd, frameIndex);
    offscreen_.endRenderPass(cmd);

    // NOTE:
    // VulkanOffscreen finalLayout is SHADER_READ_ONLY_OPTIMAL, so
    // the image can be sampled in the following fragment shader pass.
}

// -----------------------------------------------------------------
//  onRender - delegate swapchain render pass to onRenderScreen
// -----------------------------------------------------------------
void VkRendererBase::onRender(VkCommandBuffer cmd,
                               uint32_t frameIndex,
                               uint32_t imageIndex)
{
    onRenderScreen(cmd, frameIndex, imageIndex);
}

// -----------------------------------------------------------------
//  readOffscreenPixel
// -----------------------------------------------------------------
std::array<uint8_t, 4> VkRendererBase::readOffscreenPixel(uint32_t x, uint32_t y)
{
    const VkExtent2D ext = getExtent();
    if (x >= ext.width || y >= ext.height)
        return { 0, 0, 0, 0 };

    // Readback buffer size for one RGBA8 pixel.
    constexpr VkDeviceSize kPixelBytes = 4;
    if (!readbackReady_) {
        readbackBuffer_.createMapped(getContext(), kPixelBytes,
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        readbackReady_ = true;
    }

    // One-shot command buffer: copy image pixel to buffer.
    VkCommandBuffer cmd = getCommandPool().beginSingleTimeCommands();

    // Transition offscreen color image to TRANSFER_SRC layout.
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = offscreen_.getColorImage();
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy one pixel to readback buffer.
    VkBufferImageCopy region{};
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource  = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset       = { static_cast<int32_t>(x),
                                  static_cast<int32_t>(y), 0 };
    region.imageExtent       = { 1, 1, 1 };

    vkCmdCopyImageToBuffer(cmd, offscreen_.getColorImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           readbackBuffer_.get(), 1, &region);

    // Transition back to SHADER_READ_ONLY layout.
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    getCommandPool().endSingleTimeCommands(cmd);

    // Read mapped buffer on CPU.
    std::array<uint8_t, 4> result{};
    std::memcpy(result.data(), readbackBuffer_.getMapped(), kPixelBytes);
    return result;
}

} // namespace VKG
