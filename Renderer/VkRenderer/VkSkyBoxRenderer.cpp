#include "VkSkyBoxRenderer.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <array>
#include <stdexcept>

namespace Phantom::VKG {

// ---------------------------------------------------------------------------
// Unit cube vertex positions (8 corners, side length 2, centered at origin).
// ---------------------------------------------------------------------------
static const std::array<float, 24> kSkyBoxPositions = {
    -1.f, -1.f, -1.f,
     1.f, -1.f, -1.f,
     1.f,  1.f, -1.f,
    -1.f,  1.f, -1.f,
    -1.f, -1.f,  1.f,
     1.f, -1.f,  1.f,
     1.f,  1.f,  1.f,
    -1.f,  1.f,  1.f,
};

// Two triangles per face, 6 faces, 36 indices.
static const std::array<uint32_t, 36> kSkyBoxIndices = {
    // -Z
    0, 1, 2,  2, 3, 0,
    // +Z
    4, 6, 5,  6, 4, 7,
    // -X
    0, 3, 7,  7, 4, 0,
    // +X
    1, 5, 6,  6, 2, 1,
    // -Y
    0, 4, 5,  5, 1, 0,
    // +Y
    3, 2, 6,  6, 7, 3,
};

// ---------------------------------------------------------------------------

void VkSkyBoxRenderer::create(const VulkanContext& ctx,
                               const VulkanCommandPool& pool,
                               VkRenderPass renderPass,
                               uint32_t framesInFlight)
{
    framesInFlight_ = framesInFlight;
    VkDevice device = ctx.getDevice();

    // --- Descriptor Set Layout ---
    // binding 0: UBO (vertex stage)
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    // binding 1: samplerCube (fragment stage)
    VkDescriptorSetLayoutBinding cubeBinding{};
    cubeBinding.binding         = 1;
    cubeBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    cubeBinding.descriptorCount = 1;
    cubeBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    descriptorSetLayout_.create(device, { uboBinding, cubeBinding });

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          framesInFlight },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  framesInFlight },
    };
    descriptorPool_.create(device, poolSizes, framesInFlight);

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

    // --- Static geometry upload ---
    positionBuffer_.create(ctx, pool,
        kSkyBoxPositions.size() * sizeof(float),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        kSkyBoxPositions.data());

    indexBuffer_.create(ctx, pool,
        kSkyBoxIndices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        kSkyBoxIndices.data());

    // --- Pipeline ---
    std::vector<VkVertexInputBindingDescription> bindings = {
        { 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX },
    };
    std::vector<VkVertexInputAttributeDescription> attrs = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
    };

    PipelineConfig pCfg{};
    pCfg.vertSpv             = config_.vertSpv;
    pCfg.fragSpv             = config_.fragSpv;
    pCfg.bindingDescs        = bindings;
    pCfg.attrDescs           = attrs;
    pCfg.topology            = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pCfg.descriptorSetLayout = descriptorSetLayout_.get();
    // Skybox: front face is CW when viewed from inside the cube.
    pCfg.frontFace           = VK_FRONT_FACE_CLOCKWISE;
    pCfg.cullMode            = VK_CULL_MODE_NONE;
    pCfg.depthTest           = true;
    pCfg.depthWrite          = false;  // Do not overwrite depth; skybox renders at max depth.
    pCfg.depthCompareOp      = VK_COMPARE_OP_LESS_OR_EQUAL;  // Skybox outputs depth=1.0 (max).
    pCfg.samples             = config_.samples;

    pipeline_.create(ctx, renderPass, pCfg);
}

void VkSkyBoxRenderer::destroy(VkDevice device)
{
    for (auto& ub : uniformBuffers_) ub.destroy(device);
    uniformBuffers_.clear();

    positionBuffer_.destroy(device);
    indexBuffer_.destroy(device);

    pipeline_.destroy(device);
    descriptorPool_.destroy(device);
    descriptorSetLayout_.destroy(device);

    hasCubeMap_     = false;
    framesInFlight_ = 2;
}

void VkSkyBoxRenderer::setCubeMap(VkDevice device,
                                   VkImageView cubeMapView,
                                   VkSampler   sampler)
{
    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView   = cubeMapView;
    ii.sampler     = sampler;

    for (uint32_t i = 0; i < framesInFlight_; ++i) {
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = descriptorSets_[i];
        w.dstBinding      = 1;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo      = &ii;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    hasCubeMap_ = true;
}

void VkSkyBoxRenderer::upload(const Buffer& buffer, uint32_t frameIndex)
{
    UBOData ubo{ buffer.projectionMatrix, buffer.viewMatrix };
    uniformBuffers_[frameIndex].write(&ubo, sizeof(ubo));
}

void VkSkyBoxRenderer::render(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!hasCubeMap_) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkBuffer     vbuf   = positionBuffer_.getBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

    vkCmdBindIndexBuffer(cmd, indexBuffer_.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(kSkyBoxIndices.size()), 1, 0, 0, 0);
}

} // namespace Phantom::VKG
