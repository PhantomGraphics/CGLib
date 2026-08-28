#include "VkLineRenderer.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <stdexcept>

namespace Phantom::VKG {

void VkLineRenderer::create(const VulkanContext& ctx,
                             const VulkanCommandPool& pool,
                             VkRenderPass renderPass,
                             uint32_t framesInFlight)
{
    framesInFlight_ = framesInFlight;
    VkDevice device = ctx.getDevice();

    // --- Descriptor Set Layout ---
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    descriptorSetLayout_.create(device, { uboBinding });

    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight };
    descriptorPool_.create(device, { poolSize }, framesInFlight);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayout_.get());
    descriptorSets_ = descriptorPool_.allocateSets(device, layouts);

    uniformBuffers_.resize(framesInFlight);
    for (auto& ub : uniformBuffers_)
        ub.createMapped(ctx, sizeof(UBOData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        VkDescriptorBufferInfo bi{ uniformBuffers_[i].getBuffer(), 0, sizeof(UBOData) };
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = descriptorSets_[i];
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &bi;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    // --- Pipeline ---
    std::vector<VkVertexInputBindingDescription> bindings = {
        { 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX },
        { 1, sizeof(float) * 4, VK_VERTEX_INPUT_RATE_VERTEX },
    };
    std::vector<VkVertexInputAttributeDescription> attrs = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    0 },
        { 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },
    };

    PipelineConfig pCfg{};
    pCfg.vertSpv             = config_.vertSpv;
    pCfg.fragSpv             = config_.fragSpv;
    pCfg.bindingDescs        = bindings;
    pCfg.attrDescs           = attrs;
    pCfg.topology            = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    pCfg.descriptorSetLayout = descriptorSetLayout_.get();
    pCfg.cullMode            = VK_CULL_MODE_NONE;
    pCfg.lineWidth           = config_.lineWidth;
    pCfg.depthTest           = true;
    pCfg.depthWrite          = true;
    pCfg.samples             = config_.samples;

    pipeline_.create(ctx, renderPass, pCfg);
}

void VkLineRenderer::destroy(VkDevice device)
{
    for (auto& ub : uniformBuffers_) ub.destroy(device);
    uniformBuffers_.clear();

    positionBuffer_.destroy(device);
    colorBuffer_.destroy(device);
    indexBuffer_.destroy(device);

    pipeline_.destroy(device);
    descriptorPool_.destroy(device);
    descriptorSetLayout_.destroy(device);

    indexCount_ = 0;
}

void VkLineRenderer::upload(const VulkanContext& ctx,
                             const VulkanCommandPool& pool,
                             const Buffer& buffer)
{
    if (buffer.indices.empty()) { indexCount_ = 0; return; }

    VkDevice device = ctx.getDevice();

    positionBuffer_.destroy(device);
    colorBuffer_.destroy(device);
    indexBuffer_.destroy(device);

    positionBuffer_.create(ctx, pool,
        buffer.positions.size() * sizeof(float),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        buffer.positions.data());

    colorBuffer_.create(ctx, pool,
        buffer.colors.size() * sizeof(float),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        buffer.colors.data());

    indexBuffer_.create(ctx, pool,
        buffer.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        buffer.indices.data());

    indexCount_ = static_cast<uint32_t>(buffer.indices.size());

    UBOData ubo{ buffer.projectionMatrix * buffer.modelViewMatrix };
    for (auto& ub : uniformBuffers_)
        ub.write(&ubo, sizeof(ubo));
}

void VkLineRenderer::updateMVP(uint32_t frameIndex, const glm::mat4& mvp)
{
    if (frameIndex < uniformBuffers_.size()) {
        UBOData ubo{ mvp };
        uniformBuffers_[frameIndex].write(&ubo, sizeof(ubo));
    }
}

void VkLineRenderer::render(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (indexCount_ == 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkBuffer     vbufs[]   = { positionBuffer_.getBuffer(), colorBuffer_.getBuffer() };
    VkDeviceSize offsets[] = { 0, 0 };
    vkCmdBindVertexBuffers(cmd, 0, 2, vbufs, offsets);

    vkCmdBindIndexBuffer(cmd, indexBuffer_.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

} // namespace Phantom::VKG
