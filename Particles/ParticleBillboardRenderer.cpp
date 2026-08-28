#include "ParticleBillboardRenderer.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"

namespace Phantom::Particles {

void ParticleBillboardRenderer::onInit(const Phantom::VKG::VulkanContext& ctx, VkRenderPass renderPass,
                                       uint32_t framesInFlight, VkBuffer particleBuffer, uint32_t maxParticles)
{
    framesInFlight_ = framesInFlight;
    maxParticles_    = maxParticles;
    VkDevice device  = ctx.getDevice();

    // binding 0: particle SSBO (read-only from the vertex stage), binding 1: per-frame camera UBO.
    VkDescriptorSetLayoutBinding b0{};
    b0.binding         = 0;
    b0.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b0.descriptorCount = 1;
    b0.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding b1{};
    b1.binding         = 1;
    b1.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b1.descriptorCount = 1;
    b1.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    dsl_.create(device, { b0, b1 });

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, framesInFlight_ },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight_ },
    };
    descPool_.create(device, poolSizes, framesInFlight_);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight_, dsl_.get());
    descSets_ = descPool_.allocateSets(device, layouts);

    cameraUBO_.resize(framesInFlight_);
    for (auto& ubo : cameraUBO_)
        ubo.createMapped(ctx, sizeof(CameraUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    for (uint32_t f = 0; f < framesInFlight_; ++f) {
        VkDescriptorBufferInfo particleInfo{ particleBuffer, 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo cameraInfo{ cameraUBO_[f].get(), 0, sizeof(CameraUBO) };

        VkWriteDescriptorSet writes[2]{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = descSets_[f];
        writes[0].dstBinding      = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].pBufferInfo     = &particleInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = descSets_[f];
        writes[1].dstBinding      = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].pBufferInfo     = &cameraInfo;

        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    }

    Phantom::VKG::PipelineConfig cfg{};
    cfg.vertSpv             = shaders_.vertSpv;
    cfg.fragSpv             = shaders_.fragSpv;
    cfg.topology            = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    cfg.descriptorSetLayout = dsl_.get();
    cfg.cullMode            = VK_CULL_MODE_NONE;
    cfg.depthTest           = true;
    cfg.depthWrite          = false; // particles never occlude each other/opaque geometry by depth write
    cfg.blendEnable         = true;
    pipeline_.create(ctx, renderPass, cfg);
}

void ParticleBillboardRenderer::setCamera(const glm::mat4& view, const glm::mat4& proj)
{
    cameraData_.view = view;
    cameraData_.proj = proj;
    // Camera right/up in world space are the view matrix's first two rows (view rotates
    // world -> camera space, so its rows are the camera basis vectors expressed in world coords).
    cameraData_.camRight = glm::vec4(view[0][0], view[1][0], view[2][0], 0.f);
    cameraData_.camUp    = glm::vec4(view[0][1], view[1][1], view[2][1], 0.f);
}

void ParticleBillboardRenderer::onUpdate(uint32_t frameIndex)
{
    cameraUBO_[frameIndex].write(&cameraData_, sizeof(cameraData_));
}

void ParticleBillboardRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (maxParticles_ == 0)
        return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getLayout(),
                            0, 1, &descSets_[frameIndex], 0, nullptr);
    vkCmdDraw(cmd, maxParticles_ * 6u, 1, 0, 0);
}

void ParticleBillboardRenderer::onCleanup(VkDevice device)
{
    pipeline_.destroy(device);
    for (auto& ubo : cameraUBO_) ubo.destroy(device);
    cameraUBO_.clear();
    if (!descSets_.empty()) {
        descPool_.destroy(device);
        descSets_.clear();
    }
    dsl_.destroy(device);
    maxParticles_ = 0;
}

} // namespace Phantom::Particles
