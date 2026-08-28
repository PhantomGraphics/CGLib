#include "VulkanRenderPass.h"
#include "VulkanContext.h"
#include "detail/VkCheckInternal.h"

#include <array>

namespace Phantom::VKG {

bool VulkanRenderPass::create(const VulkanContext& ctx,
                               VkFormat colorFormat, VkFormat depthFormat,
                               VkSampleCountFlagBits samples)
{
    samples_ = samples;

    // Common subpass dependency (same for both MSAA and non-MSAA paths).
    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    if (samples == VK_SAMPLE_COUNT_1_BIT) {
        // ---- Non-MSAA path ----
        VkAttachmentDescription colorAtt{};
        colorAtt.format         = colorFormat;
        colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAtt{};
        depthAtt.format         = depthFormat;
        depthAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        std::array<VkAttachmentDescription, 2> attachments = { colorAtt, depthAtt };

        VkRenderPassCreateInfo rpci{};
        rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = (uint32_t)attachments.size();
        rpci.pAttachments    = attachments.data();
        rpci.subpassCount    = 1;
        rpci.pSubpasses      = &subpass;
        rpci.dependencyCount = 1;
        rpci.pDependencies   = &dep;

        VKG_CHECK(vkCreateRenderPass(ctx.getDevice(), &rpci, nullptr, &renderPass_),
                  "Failed to create render pass", false);

    } else {
        // ---- MSAA path ----
        // Attachment 0: multisample color (intermediate, not presented)
        VkAttachmentDescription msaaColorAtt{};
        msaaColorAtt.format         = colorFormat;
        msaaColorAtt.samples        = samples;
        msaaColorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        msaaColorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE; // discarded after resolve
        msaaColorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        msaaColorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaColorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        msaaColorAtt.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Attachment 1: multisample depth
        VkAttachmentDescription depthAtt{};
        depthAtt.format         = depthFormat;
        depthAtt.samples        = samples;
        depthAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAtt.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Attachment 2: resolve target (swapchain, 1-sample)
        VkAttachmentDescription resolveAtt{};
        resolveAtt.format         = colorFormat;
        resolveAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        resolveAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef  { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL         };
        VkAttachmentReference depthRef  { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        VkAttachmentReference resolveRef{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL         };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = 1;
        subpass.pColorAttachments       = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;
        subpass.pResolveAttachments     = &resolveRef;

        std::array<VkAttachmentDescription, 3> attachments = { msaaColorAtt, depthAtt, resolveAtt };

        VkRenderPassCreateInfo rpci{};
        rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = (uint32_t)attachments.size();
        rpci.pAttachments    = attachments.data();
        rpci.subpassCount    = 1;
        rpci.pSubpasses      = &subpass;
        rpci.dependencyCount = 1;
        rpci.pDependencies   = &dep;

        VKG_CHECK(vkCreateRenderPass(ctx.getDevice(), &rpci, nullptr, &renderPass_),
                  "Failed to create MSAA render pass", false);
    }
    return true;
}

void VulkanRenderPass::destroy(VkDevice device) {
    if (renderPass_) {
        vkDestroyRenderPass(device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
}

} // namespace VKG
