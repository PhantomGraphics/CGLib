#include "VkTexRenderer.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <stdexcept>

namespace Phantom::VKG {

void VkTexRenderer::create(const VulkanContext& ctx,
                            const VulkanCommandPool& /*pool*/,
                            VkRenderPass renderPass,
                            uint32_t framesInFlight)
{
    framesInFlight_ = framesInFlight;
    VkDevice device = ctx.getDevice();

    // Descriptor Set Layout: binding 0 = combined image sampler (fragment stage)
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding         = 0;
    samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    descriptorSetLayout_.create(device, { samplerBinding });

    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, framesInFlight };
    descriptorPool_.create(device, { poolSize }, framesInFlight);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayout_.get());
    descriptorSets_ = descriptorPool_.allocateSets(device, layouts);

    // Pipeline: no vertex buffers, no depth test (full-screen blit)
    PipelineConfig pCfg{};
    pCfg.vertSpv             = config_.vertSpv;
    pCfg.fragSpv             = config_.fragSpv;
    pCfg.bindingDescs        = {};   // no vertex buffers
    pCfg.attrDescs           = {};
    pCfg.topology            = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pCfg.descriptorSetLayout = descriptorSetLayout_.get();
    pCfg.cullMode            = VK_CULL_MODE_NONE;
    pCfg.depthTest           = false;
    pCfg.depthWrite          = false;
    pCfg.samples             = config_.samples;

    pipeline_.create(ctx, renderPass, pCfg);
}

void VkTexRenderer::destroy(VkDevice device)
{
    pipeline_.destroy(device);
    descriptorPool_.destroy(device);
    descriptorSetLayout_.destroy(device);
    hasTexture_     = false;
    framesInFlight_ = 2;
}

void VkTexRenderer::setTexture(VkDevice    device,
                                VkImageView imageView,
                                VkSampler   sampler,
                                uint32_t    frameIndex)
{
    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView   = imageView;
    ii.sampler     = sampler;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = descriptorSets_[frameIndex];
    w.dstBinding      = 0;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &ii;

    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);

    hasTexture_ = true;
}

void VkTexRenderer::render(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!hasTexture_) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    // No vertex buffer: shader generates a full-screen quad from gl_VertexIndex.
    vkCmdDraw(cmd, 6, 1, 0, 0);
}

} // namespace Phantom::VKG
