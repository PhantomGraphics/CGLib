#pragma once

#include <vulkan/vulkan.h>

namespace Phantom::VKG {

class VulkanContext;

/// @brief Creates and owns a standard single-subpass render pass with one color and one depth attachment.
///
/// The color attachment uses VK_ATTACHMENT_LOAD_OP_CLEAR and transitions to
/// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR at the end of the pass.
/// The depth attachment uses VK_ATTACHMENT_LOAD_OP_CLEAR with an optimal layout.
///
/// Usage:
/// @code
///   VulkanRenderPass rp;
///   VkFormat depth = swapChain.findDepthFormat();
///   rp.create(ctx, swapChain.getImageFormat(), depth);
///   // ... use rp.get() to create pipelines and framebuffers ...
///   rp.destroy(ctx.getDevice());
/// @endcode
class VulkanRenderPass {
public:
    VulkanRenderPass() = default;
    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
    ~VulkanRenderPass() = default;

    /// @brief Creates the render pass.
    ///
    /// @param ctx         Logical device context.
    /// @param colorFormat Swap-chain color format (e.g. VK_FORMAT_B8G8R8A8_SRGB).
    /// @param depthFormat Depth/stencil format (e.g. VK_FORMAT_D32_SFLOAT).
    /// @param samples     MSAA sample count.  When > 1, a resolve attachment is added
    ///                    and the framebuffer must supply a multisampled color image.
    ///                    Pass VK_SAMPLE_COUNT_1_BIT (default) for no MSAA.
    /// @return false if render pass creation fails.
    bool create(const VulkanContext& ctx,
                VkFormat colorFormat, VkFormat depthFormat,
                VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);

    /// @brief Destroys the render pass.
    /// @param device Logical device that owns the render pass.
    void destroy(VkDevice device);

    /// @brief Returns the underlying VkRenderPass handle.
    VkRenderPass get() const { return renderPass_; }

    /// @brief Returns the sample count this render pass was created with.
    VkSampleCountFlagBits getSamples() const { return samples_; }

private:
    VkRenderPass          renderPass_ = VK_NULL_HANDLE;
    VkSampleCountFlagBits samples_    = VK_SAMPLE_COUNT_1_BIT;
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
