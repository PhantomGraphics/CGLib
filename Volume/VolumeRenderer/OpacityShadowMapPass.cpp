#include "OpacityShadowMapPass.h"
#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

#include <cstdio>

namespace Phantom::Volume {

namespace {
constexpr VkFormat kFormat = VK_FORMAT_R32_SFLOAT;
}

bool OpacityShadowMapPass::create(const Phantom::VKG::VulkanContext& ctx, uint32_t size, uint32_t layerCount) {
    if (size == 0 || layerCount == 0) {
        std::fprintf(stderr, "[Volume] OpacityShadowMapPass: size and layerCount must be > 0\n");
        return false;
    }

    size_ = size;
    layerCount_ = layerCount;
    VkDevice device = ctx.getDevice();

    // --- Image (N array layers, one R32F "cumulative density" plane per layer) ---
    VkImageCreateInfo imageCI{};
    imageCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType     = VK_IMAGE_TYPE_2D;
    imageCI.extent        = { size_, size_, 1 };
    imageCI.mipLevels     = 1;
    imageCI.arrayLayers   = layerCount_;
    imageCI.format        = kFormat;
    imageCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCI.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCI.samples       = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageCI, nullptr, &image_) != VK_SUCCESS) {
        std::fprintf(stderr, "[Volume] OpacityShadowMapPass: failed to create image\n");
        return false;
    }

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, image_, &req);
    auto memType = ctx.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memType) {
        std::fprintf(stderr, "[Volume] OpacityShadowMapPass: no suitable memory type\n");
        destroy(ctx);
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = req.size;
    allocInfo.memoryTypeIndex = *memType;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        std::fprintf(stderr, "[Volume] OpacityShadowMapPass: failed to allocate image memory\n");
        destroy(ctx);
        return false;
    }
    vkBindImageMemory(device, image_, memory_, 0);

    // --- Array view (sampling, all layers) ---
    VkImageViewCreateInfo arrayViewCI{};
    arrayViewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    arrayViewCI.image                           = image_;
    arrayViewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    arrayViewCI.format                          = kFormat;
    arrayViewCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    arrayViewCI.subresourceRange.baseMipLevel   = 0;
    arrayViewCI.subresourceRange.levelCount     = 1;
    arrayViewCI.subresourceRange.baseArrayLayer = 0;
    arrayViewCI.subresourceRange.layerCount     = layerCount_;

    if (vkCreateImageView(device, &arrayViewCI, nullptr, &arrayView_) != VK_SUCCESS) {
        std::fprintf(stderr, "[Volume] OpacityShadowMapPass: failed to create array view\n");
        destroy(ctx);
        return false;
    }

    // --- Per-layer views (render targets, one layer each) ---
    layerViews_.resize(layerCount_, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < layerCount_; ++i) {
        VkImageViewCreateInfo layerViewCI = arrayViewCI;
        layerViewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        layerViewCI.subresourceRange.baseArrayLayer = i;
        layerViewCI.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(device, &layerViewCI, nullptr, &layerViews_[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "[Volume] OpacityShadowMapPass: failed to create layer view %u\n", i);
            destroy(ctx);
            return false;
        }
    }

    // --- Render pass: single color attachment, no depth ---
    VkAttachmentDescription colorAttach{};
    colorAttach.format         = kFormat;
    colorAttach.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttach.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttach.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttach.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep0{};
    dep0.srcSubpass      = VK_SUBPASS_EXTERNAL;
    dep0.dstSubpass      = 0;
    dep0.srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep0.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep0.srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dep0.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep0.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDependency dep1{};
    dep1.srcSubpass      = 0;
    dep1.dstSubpass      = VK_SUBPASS_EXTERNAL;
    dep1.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep1.dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep1.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep1.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dep1.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDependency deps[] = { dep0, dep1 };

    VkRenderPassCreateInfo rpCI{};
    rpCI.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCI.attachmentCount = 1;
    rpCI.pAttachments    = &colorAttach;
    rpCI.subpassCount    = 1;
    rpCI.pSubpasses      = &subpass;
    rpCI.dependencyCount = 2;
    rpCI.pDependencies   = deps;

    if (vkCreateRenderPass(device, &rpCI, nullptr, &renderPass_) != VK_SUCCESS) {
        std::fprintf(stderr, "[Volume] OpacityShadowMapPass: failed to create render pass\n");
        destroy(ctx);
        return false;
    }

    // --- Framebuffers (one per layer) ---
    framebuffers_.resize(layerCount_, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < layerCount_; ++i) {
        VkFramebufferCreateInfo fbCI{};
        fbCI.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCI.renderPass      = renderPass_;
        fbCI.attachmentCount = 1;
        fbCI.pAttachments    = &layerViews_[i];
        fbCI.width           = size_;
        fbCI.height          = size_;
        fbCI.layers          = 1;
        if (vkCreateFramebuffer(device, &fbCI, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "[Volume] OpacityShadowMapPass: failed to create framebuffer %u\n", i);
            destroy(ctx);
            return false;
        }
    }

    // Edge texels stay at the layer clear value (0 = no density) as long as the light
    // ortho frustum comfortably contains the volume bounds (see PBVRRenderer::computeLightProj),
    // so CLAMP_TO_EDGE is sufficient and avoids VulkanSampler's hardcoded INT border color
    // (which would be a format mismatch against this R32_SFLOAT image).
    if (!sampler_.create(device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)) {
        std::fprintf(stderr, "[Volume] OpacityShadowMapPass: failed to create sampler\n");
        destroy(ctx);
        return false;
    }

    return true;
}

void OpacityShadowMapPass::destroy(const Phantom::VKG::VulkanContext& ctx) {
    VkDevice device = ctx.getDevice();

    sampler_.destroy(device);

    for (VkFramebuffer fb : framebuffers_) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device, fb, nullptr);
    }
    framebuffers_.clear();

    if (renderPass_ != VK_NULL_HANDLE) { vkDestroyRenderPass(device, renderPass_, nullptr); renderPass_ = VK_NULL_HANDLE; }

    for (VkImageView view : layerViews_) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
    }
    layerViews_.clear();

    if (arrayView_ != VK_NULL_HANDLE) { vkDestroyImageView(device, arrayView_, nullptr); arrayView_ = VK_NULL_HANDLE; }
    if (image_     != VK_NULL_HANDLE) { vkDestroyImage(device, image_, nullptr);         image_     = VK_NULL_HANDLE; }
    if (memory_    != VK_NULL_HANDLE) { vkFreeMemory(device, memory_, nullptr);          memory_    = VK_NULL_HANDLE; }

    size_ = 0;
    layerCount_ = 0;
}

void OpacityShadowMapPass::beginLayer(VkCommandBuffer cmd, uint32_t layerIndex) const {
    VkClearValue clear{};
    clear.color = {{ 0.0f, 0.0f, 0.0f, 0.0f }};

    VkRenderPassBeginInfo bi{};
    bi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    bi.renderPass        = renderPass_;
    bi.framebuffer       = framebuffers_[layerIndex];
    bi.renderArea.offset = { 0, 0 };
    bi.renderArea.extent = { size_, size_ };
    bi.clearValueCount   = 1;
    bi.pClearValues      = &clear;

    vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = static_cast<float>(size_);
    vp.height   = static_cast<float>(size_);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{ { 0, 0 }, { size_, size_ } };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void OpacityShadowMapPass::endLayer(VkCommandBuffer cmd) const {
    vkCmdEndRenderPass(cmd);
}

} // namespace Phantom::Volume
