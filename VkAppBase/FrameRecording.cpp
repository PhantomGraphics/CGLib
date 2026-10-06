#include "FrameRecording.h"

namespace VKG {

std::array<VkClearValue, 2> swapchainClearValues() {
    std::array<VkClearValue, 2> v{};
    v[0].color        = {{0.05f, 0.05f, 0.05f, 1.f}};
    v[1].depthStencil = {1.f, 0};
    return v;
}

VkViewport fullViewport(VkExtent2D extent) {
    VkViewport vp{};
    vp.x        = 0.f;
    vp.y        = 0.f;
    vp.width    = static_cast<float>(extent.width);
    vp.height   = static_cast<float>(extent.height);
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    return vp;
}

VkRect2D fullScissor(VkExtent2D extent) {
    return VkRect2D{{0, 0}, extent};
}

void beginSwapchainRenderPass(VkCommandBuffer cmd, VkRenderPass renderPass,
                              VkFramebuffer framebuffer, VkExtent2D extent) {
    const auto clearValues = swapchainClearValues();

    VkRenderPassBeginInfo rp{};
    rp.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass        = renderPass;
    rp.framebuffer       = framebuffer;
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = extent;
    rp.clearValueCount   = static_cast<uint32_t>(clearValues.size());
    rp.pClearValues      = clearValues.data();
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport vp = fullViewport(extent);
    const VkRect2D   sc = fullScissor(extent);
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
}

} // namespace VKG
