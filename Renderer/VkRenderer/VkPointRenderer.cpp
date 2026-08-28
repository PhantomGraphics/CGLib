#include "VkPointRenderer.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <stdexcept>

namespace Phantom::VKG {

// -----------------------------------------------------------------
//  create
// -----------------------------------------------------------------
void VkPointRenderer::create(const VulkanContext& ctx,
                              const VulkanCommandPool& pool,
                              VkRenderPass renderPass,
                              uint32_t framesInFlight)
{
    framesInFlight_ = framesInFlight;
    VkDevice device = ctx.getDevice();

    // --- Descriptor Set Layout (binding 0: UBO) ---
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    descriptorSetLayout_.create(device, { uboBinding });

    // --- Descriptor Pool ---
    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight };
    descriptorPool_.create(device, { poolSize }, framesInFlight);

    // --- Descriptor Sets (one per frame) ---
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayout_.get());
    descriptorSets_ = descriptorPool_.allocateSets(device, layouts);

    // --- Uniform Buffers (one per frame, persistently mapped) ---
    uniformBuffers_.resize(framesInFlight);
    for (auto& ub : uniformBuffers_)
        ub.createMapped(ctx, sizeof(UBOData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // Write descriptor sets
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = uniformBuffers_[i].getBuffer();
        bi.offset = 0;
        bi.range  = sizeof(UBOData);

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
    // Binding descriptions: 3 separate vertex buffers
    std::vector<VkVertexInputBindingDescription> bindings = {
        { 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX }, // position
        { 1, sizeof(float) * 4, VK_VERTEX_INPUT_RATE_VERTEX }, // color
        { 2, sizeof(float),     VK_VERTEX_INPUT_RATE_VERTEX }, // size
    };
    std::vector<VkVertexInputAttributeDescription> attrs = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, // position
        { 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 }, // color
        { 2, 2, VK_FORMAT_R32_SFLOAT, 0 },           // size
    };

    PipelineConfig pCfg{};
    pCfg.vertSpv             = config_.vertSpv;
    pCfg.fragSpv             = config_.fragSpv;
    pCfg.bindingDescs        = bindings;
    pCfg.attrDescs           = attrs;
    pCfg.topology            = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    pCfg.descriptorSetLayout = descriptorSetLayout_.get();
    pCfg.cullMode            = VK_CULL_MODE_NONE;
    pCfg.depthTest           = true;
    pCfg.depthWrite          = true;
    pCfg.samples             = config_.samples;

    pipeline_.create(ctx, renderPass, pCfg);
}

// -----------------------------------------------------------------
//  destroy
// -----------------------------------------------------------------
void VkPointRenderer::destroy(VkDevice device)
{
    for (auto& ub : uniformBuffers_) ub.destroy(device);
    uniformBuffers_.clear();

    positionBuffer_.destroy(device);
    colorBuffer_.destroy(device);
    sizeBuffer_.destroy(device);

    pipeline_.destroy(device);
    descriptorPool_.destroy(device);
    descriptorSetLayout_.destroy(device);

    pointCount_ = 0;
}

// -----------------------------------------------------------------
//  upload
// -----------------------------------------------------------------
void VkPointRenderer::upload(const VulkanContext& ctx,
                              const VulkanCommandPool& pool,
                              const Buffer& buffer)
{
    const uint32_t n = static_cast<uint32_t>(buffer.positions.size() / 3);
    if (n == 0) { pointCount_ = 0; return; }

    VkDevice device = ctx.getDevice();

    positionBuffer_.destroy(device);
    colorBuffer_.destroy(device);
    sizeBuffer_.destroy(device);

    positionBuffer_.create(ctx, pool,
        buffer.positions.size() * sizeof(float),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        buffer.positions.data());

    colorBuffer_.create(ctx, pool,
        buffer.colors.size() * sizeof(float),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        buffer.colors.data());

    sizeBuffer_.create(ctx, pool,
        buffer.sizes.size() * sizeof(float),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        buffer.sizes.data());

    pointCount_ = n;

    // Store MVP for all frames (will be written per-frame in render())
    // Save matrices for UBO update
    for (uint32_t i = 0; i < framesInFlight_; ++i) {
        UBOData ubo{ buffer.projectionMatrix * buffer.modelViewMatrix };
        uniformBuffers_[i].write(&ubo, sizeof(ubo));
    }
}

// -----------------------------------------------------------------
//  render
// -----------------------------------------------------------------
void VkPointRenderer::render(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (pointCount_ == 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkBuffer     vbufs[]   = { positionBuffer_.getBuffer(),
                                colorBuffer_.getBuffer(),
                                sizeBuffer_.getBuffer() };
    VkDeviceSize offsets[] = { 0, 0, 0 };
    vkCmdBindVertexBuffers(cmd, 0, 3, vbufs, offsets);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    vkCmdDraw(cmd, pointCount_, 1, 0, 0);
}

} // namespace Phantom::VKG
