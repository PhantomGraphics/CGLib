#include "LinePipeline.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

namespace VkVolumeView {

void LinePipeline::create(const Phantom::VKG::VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
                             std::vector<uint32_t> vertSpv, std::vector<uint32_t> fragSpv) {
    VkDevice device = ctx.getDevice();

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    dsl_.create(device, {uboBinding});

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight};
    pool_.create(device, {poolSize}, framesInFlight);

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

    std::vector<VkVertexInputBindingDescription> bindings = {
        {0, sizeof(LineVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    std::vector<VkVertexInputAttributeDescription> attrs = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(LineVertex, pos))},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(LineVertex, color))},
    };

    Phantom::VKG::PipelineConfig cfg{};
    cfg.vertSpv = std::move(vertSpv);
    cfg.fragSpv = std::move(fragSpv);
    cfg.bindingDescs = bindings;
    cfg.attrDescs = attrs;
    cfg.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    cfg.descriptorSetLayout = dsl_.get();
    cfg.cullMode = VK_CULL_MODE_NONE;
    cfg.lineWidth = 1.0f;
    cfg.depthTest = true;
    cfg.depthWrite = true;
    cfg.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    pipeline_.create(ctx, renderPass, cfg);
}

void LinePipeline::destroy(VkDevice device) {
    for (auto& ubo : ubos_) {
        ubo.destroy(device);
    }
    ubos_.clear();
    sets_.clear();

    pipeline_.destroy(device);
    pool_.destroy(device);
    dsl_.destroy(device);
}

void LinePipeline::updateUBO(uint32_t frameIndex, const UBO& ubo) {
    if (frameIndex < ubos_.size()) {
        ubos_[frameIndex].write(&ubo, sizeof(UBO));
    }
}

} // namespace VkVolumeView
