#include "VulkanOffscreen.h"
#include "VulkanContext.h"
#include "VulkanImage.h"
#include "detail/VkCheckInternal.h"

namespace Phantom::VKG {

bool VulkanOffscreen::create(const VulkanContext& ctx,
                              uint32_t width, uint32_t height,
                              VkFormat colorFormat,
                              VkFormat depthFormat)
{
    extent_      = { width, height };
    colorFormat_ = colorFormat;
    VkDevice device = ctx.getDevice();

    // --- カラーアタッチメント ---
    // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT: レンダーパスへ書き込み
    // VK_IMAGE_USAGE_SAMPLED_BIT         : 後続パスでサンプリング可能
    if (!VulkanImage::create(ctx, width, height, colorFormat,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             colorImage_, colorMemory_))
        return false;

    colorView_ = VulkanImage::createView(device, colorImage_,
                                         colorFormat,
                                         VK_IMAGE_ASPECT_COLOR_BIT);
    if (colorView_ == VK_NULL_HANDLE) {
        vkDestroyImage(device, colorImage_, nullptr);
        vkFreeMemory(device, colorMemory_, nullptr);
        colorImage_ = VK_NULL_HANDLE;
        colorMemory_ = VK_NULL_HANDLE;
        return false;
    }

    // --- デプスアタッチメント ---
    // VK_IMAGE_USAGE_SAMPLED_BIT: 後続パスでサンプリング可能(シャドウマップ用途)
    if (!VulkanImage::create(ctx, width, height, depthFormat,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             depthImage_, depthMemory_)) {
        vkDestroyImageView(device, colorView_, nullptr);
        vkDestroyImage(device, colorImage_, nullptr);
        vkFreeMemory(device, colorMemory_, nullptr);
        colorView_ = VK_NULL_HANDLE;
        colorImage_ = VK_NULL_HANDLE;
        colorMemory_ = VK_NULL_HANDLE;
        return false;
    }

    depthView_ = VulkanImage::createView(device, depthImage_,
                                          depthFormat,
                                          VK_IMAGE_ASPECT_DEPTH_BIT);
    if (depthView_ == VK_NULL_HANDLE) {
        vkDestroyImageView(device, colorView_, nullptr);
        vkDestroyImage(device, colorImage_, nullptr);
        vkFreeMemory(device, colorMemory_, nullptr);
        vkDestroyImage(device, depthImage_, nullptr);
        vkFreeMemory(device, depthMemory_, nullptr);
        colorView_ = VK_NULL_HANDLE;
        colorImage_ = VK_NULL_HANDLE;
        colorMemory_ = VK_NULL_HANDLE;
        depthImage_ = VK_NULL_HANDLE;
        depthMemory_ = VK_NULL_HANDLE;
        return false;
    }

    // --- RenderPass ---
    // カラー: UNDEFINED → COLOR_ATTACHMENT → SHADER_READ_ONLY (サンプリング用)
    // デプス: UNDEFINED → DEPTH_STENCIL_ATTACHMENT → DEPTH_STENCIL_READ_ONLY (サンプリング用)
    VkAttachmentDescription colorAttach{};
    colorAttach.format         = colorFormat;
    colorAttach.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttach.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttach.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttach.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttach{};
    depthAttach.format         = depthFormat;
    depthAttach.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttach.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttach.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttach.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttach.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttach.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // 外部 → サブパス0: カラー書き込み開始前の依存
    VkSubpassDependency dep0{};
    dep0.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep0.dstSubpass    = 0;
    dep0.srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep0.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                       | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep0.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dep0.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                       | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep0.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // サブパス0 → 外部: カラー/デプス書き込み完了後サンプリング可能にする依存
    // (デプスも SAMPLED_BIT 付きで作成しているため、シャドウマップ用途でそのままサンプリング可能)
    VkSubpassDependency dep1{};
    dep1.srcSubpass    = 0;
    dep1.dstSubpass    = VK_SUBPASS_EXTERNAL;
    dep1.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                       | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep1.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep1.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                       | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dep1.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkAttachmentDescription attachments[] = { colorAttach, depthAttach };
    VkSubpassDependency      deps[]        = { dep0, dep1 };

    VkRenderPassCreateInfo rpCI{};
    rpCI.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCI.attachmentCount = 2;
    rpCI.pAttachments    = attachments;
    rpCI.subpassCount    = 1;
    rpCI.pSubpasses      = &subpass;
    rpCI.dependencyCount = 2;
    rpCI.pDependencies   = deps;

    if (vkCreateRenderPass(device, &rpCI, nullptr, &renderPass_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanOffscreen: Failed to create render pass\n");
        vkDestroyImageView(device, colorView_, nullptr);
        vkDestroyImage(device, colorImage_, nullptr);
        vkFreeMemory(device, colorMemory_, nullptr);
        vkDestroyImageView(device, depthView_, nullptr);
        vkDestroyImage(device, depthImage_, nullptr);
        vkFreeMemory(device, depthMemory_, nullptr);
        colorView_ = VK_NULL_HANDLE;
        colorImage_ = VK_NULL_HANDLE;
        colorMemory_ = VK_NULL_HANDLE;
        depthView_ = VK_NULL_HANDLE;
        depthImage_ = VK_NULL_HANDLE;
        depthMemory_ = VK_NULL_HANDLE;
        return false;
    }

    // --- Framebuffer ---
    VkImageView fbViews[] = { colorView_, depthView_ };

    VkFramebufferCreateInfo fbCI{};
    fbCI.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCI.renderPass      = renderPass_;
    fbCI.attachmentCount = 2;
    fbCI.pAttachments    = fbViews;
    fbCI.width           = width;
    fbCI.height          = height;
    fbCI.layers          = 1;

    if (vkCreateFramebuffer(device, &fbCI, nullptr, &framebuffer_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanOffscreen: Failed to create framebuffer\n");
        vkDestroyRenderPass(device, renderPass_, nullptr);
        vkDestroyImageView(device, colorView_, nullptr);
        vkDestroyImage(device, colorImage_, nullptr);
        vkFreeMemory(device, colorMemory_, nullptr);
        vkDestroyImageView(device, depthView_, nullptr);
        vkDestroyImage(device, depthImage_, nullptr);
        vkFreeMemory(device, depthMemory_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
        colorView_ = VK_NULL_HANDLE;
        colorImage_ = VK_NULL_HANDLE;
        colorMemory_ = VK_NULL_HANDLE;
        depthView_ = VK_NULL_HANDLE;
        depthImage_ = VK_NULL_HANDLE;
        depthMemory_ = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void VulkanOffscreen::destroy(const VulkanContext& ctx)
{
    VkDevice device = ctx.getDevice();

    if (framebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, framebuffer_, nullptr);
        framebuffer_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    if (colorView_   != VK_NULL_HANDLE) { vkDestroyImageView(device, colorView_,   nullptr); colorView_   = VK_NULL_HANDLE; }
    if (colorImage_  != VK_NULL_HANDLE) { vkDestroyImage    (device, colorImage_,  nullptr); colorImage_  = VK_NULL_HANDLE; }
    if (colorMemory_ != VK_NULL_HANDLE) { vkFreeMemory      (device, colorMemory_, nullptr); colorMemory_ = VK_NULL_HANDLE; }

    if (depthView_   != VK_NULL_HANDLE) { vkDestroyImageView(device, depthView_,   nullptr); depthView_   = VK_NULL_HANDLE; }
    if (depthImage_  != VK_NULL_HANDLE) { vkDestroyImage    (device, depthImage_,  nullptr); depthImage_  = VK_NULL_HANDLE; }
    if (depthMemory_ != VK_NULL_HANDLE) { vkFreeMemory      (device, depthMemory_, nullptr); depthMemory_ = VK_NULL_HANDLE; }

    extent_      = {};
    colorFormat_ = VK_FORMAT_UNDEFINED;
}

void VulkanOffscreen::beginRenderPass(VkCommandBuffer cmd,
                                       const std::array<float, 4>& clearColor,
                                       float clearDepth) const
{
    VkClearValue clears[2];
    clears[0].color        = {{ clearColor[0], clearColor[1], clearColor[2], clearColor[3] }};
    clears[1].depthStencil = { clearDepth, 0 };

    VkRenderPassBeginInfo bi{};
    bi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    bi.renderPass        = renderPass_;
    bi.framebuffer       = framebuffer_;
    bi.renderArea.offset = { 0, 0 };
    bi.renderArea.extent = extent_;
    bi.clearValueCount   = 2;
    bi.pClearValues      = clears;

    vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x        = 0.f;
    vp.y        = 0.f;
    vp.width    = static_cast<float>(extent_.width);
    vp.height   = static_cast<float>(extent_.height);
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{{ 0, 0 }, extent_};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanOffscreen::endRenderPass(VkCommandBuffer cmd) const
{
    vkCmdEndRenderPass(cmd);
}

} // namespace VKG
