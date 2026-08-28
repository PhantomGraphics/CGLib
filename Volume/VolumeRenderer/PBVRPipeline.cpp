#include "PBVRPipeline.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

namespace Phantom::Volume {

void PBVRPipeline::create(const ::VKG::VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
                             std::vector<uint32_t> vertSpv, std::vector<uint32_t> fragSpv,
                             bool enableAlphaBlend, bool enableShadowSampler) {
    const VkDevice device = ctx.getDevice();
    device_ = device;
    shadowSamplerEnabled_ = enableShadowSampler;

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    // Also readable from the fragment stage for the self-shadow transmittance lookup below.
    // GSView's gs_pbvr.frag never declares a UBO block, so this widened stage mask is a no-op there.
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> bindings = { uboBinding };
    std::vector<VkDescriptorPoolSize> poolSizes = { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight} };

    if (shadowSamplerEnabled_) {
        VkDescriptorSetLayoutBinding shadowBinding{};
        shadowBinding.binding = 1;
        shadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowBinding.descriptorCount = 1;
        shadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(shadowBinding);
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, framesInFlight});
    }

    dsl_.create(device, bindings);
    pool_.create(device, poolSizes, framesInFlight);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, dsl_.get());
    sets_ = pool_.allocateSets(device, layouts);

    ubos_.resize(framesInFlight);
    for (auto& ubo : ubos_) {
        ubo.createMapped(ctx, sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = ubos_[i].getBuffer();
        bi.offset = 0;
        bi.range = sizeof(UBO);

        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = sets_[i];
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }
    // Binding 1 (shadow sampler), if enabled, is left unwritten here -- the caller (PBVRRenderer)
    // must call updateShadowMap() for every frame in flight once the Opacity Shadow Map exists,
    // before any draw uses this descriptor set (see the class-level comment on create()).

    std::vector<VkVertexInputBindingDescription> vtxBindings = {
        {0, sizeof(PBVRVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    std::vector<VkVertexInputAttributeDescription> attrs = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(PBVRVertex, pos))},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(PBVRVertex, color))},
    };

    ::VKG::PipelineConfig cfg{};
    cfg.vertSpv = std::move(vertSpv);
    cfg.fragSpv = std::move(fragSpv);
    cfg.bindingDescs = vtxBindings;
    cfg.attrDescs = attrs;
    cfg.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    cfg.descriptorSetLayout = dsl_.get();
    cfg.cullMode = VK_CULL_MODE_NONE;
    cfg.depthTest = true;
    cfg.depthWrite = true;
    cfg.blendEnable = enableAlphaBlend;

    pipeline_.create(ctx, renderPass, cfg);
}

void PBVRPipeline::destroy(VkDevice device) {
    for (auto& ubo : ubos_) {
        ubo.destroy(device);
    }
    ubos_.clear();
    sets_.clear();

    pipeline_.destroy(device);
    pool_.destroy(device);
    dsl_.destroy(device);
}

void PBVRPipeline::updateUBO(uint32_t frameIndex, const UBO& ubo) {
    if (frameIndex < ubos_.size()) {
        ubos_[frameIndex].write(&ubo, sizeof(UBO));
    }
}

void PBVRPipeline::updateShadowMap(uint32_t frameIndex, VkImageView arrayView, VkSampler sampler) {
    if (!shadowSamplerEnabled_ || frameIndex >= sets_.size()) return;

    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView   = arrayView;
    ii.sampler     = sampler;

    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = sets_[frameIndex];
    w.dstBinding = 1;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &ii;

    // The caller is responsible for not rewriting a set while a command buffer that references
    // it is still in flight (PBVRRenderer::renderShadowDeposit() calls vkDeviceWaitIdle() first).
    vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
}

} // namespace PBVR
