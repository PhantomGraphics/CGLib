#include "ParticleSimulator.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"

namespace Phantom::Particles {

void ParticleSimulator::create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                uint32_t maxParticles, uint32_t framesInFlight)
{
    framesInFlight_ = framesInFlight;
    VkDevice device = ctx.getDevice();

    pool_.create(ctx, pool, maxParticles);

    // Descriptor set layout: binding 0 = particle SSBO (read/write), binding 1 = per-frame SimParams UBO.
    VkDescriptorSetLayoutBinding b0{};
    b0.binding         = 0;
    b0.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b0.descriptorCount = 1;
    b0.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding b1{};
    b1.binding         = 1;
    b1.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b1.descriptorCount = 1;
    b1.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    dsl_.create(device, { b0, b1 });

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, framesInFlight_ },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight_ },
    };
    descPool_.create(device, poolSizes, framesInFlight_);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight_, dsl_.get());
    descSets_ = descPool_.allocateSets(device, layouts);

    simUBO_.resize(framesInFlight_);
    for (auto& ubo : simUBO_)
        ubo.createMapped(ctx, sizeof(SimUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    for (uint32_t f = 0; f < framesInFlight_; ++f) {
        VkDescriptorBufferInfo particleInfo{ pool_.getBuffer(), 0, VK_WHOLE_SIZE };
        VkDescriptorBufferInfo uboInfo{ simUBO_[f].get(), 0, sizeof(SimUBO) };

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
        writes[1].pBufferInfo     = &uboInfo;

        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    }

    Phantom::VKG::ComputePipelineConfig cfg{};
    cfg.compSpv             = shaders_.updateCompSpv;
    cfg.descriptorSetLayout = dsl_.get();
    pipeline_.create(ctx, cfg);
}

void ParticleSimulator::destroy(VkDevice device)
{
    pipeline_.destroy(device);
    for (auto& ubo : simUBO_) ubo.destroy(device);
    simUBO_.clear();
    if (!descSets_.empty()) {
        descPool_.destroy(device);
        descSets_.clear();
    }
    dsl_.destroy(device);
    pool_.destroy(device);
    framesInFlight_ = 0;
}

void ParticleSimulator::recordUpdate(VkCommandBuffer cmd, uint32_t frameIndex, float dt)
{
    const uint32_t maxParticles = pool_.getMaxParticles();
    if (maxParticles == 0)
        return;

    const EmitterParams&           p     = emitter_.params();
    const ParticleEmitter::EmitBatch batch = emitter_.update(dt, maxParticles);

    SimUBO ubo{};
    ubo.originDt    = glm::vec4(p.origin, dt);
    ubo.gravityDrag = glm::vec4(p.gravity, p.drag);
    ubo.initialColor = glm::vec4(p.initialColor, 1.f);
    ubo.endColor      = glm::vec4(p.endColor, 1.f);
    ubo.speedParams   = glm::vec4(p.speed, p.speedVariance, p.lifeTime, p.size);
    ubo.counts        = glm::uvec4(maxParticles, batch.start, batch.count, batch.seed);
    ubo.shapeParams   = glm::vec4(p.shapeRadius, static_cast<float>(static_cast<uint32_t>(p.shape)), 0.f, 0.f);
    simUBO_[frameIndex].write(&ubo, sizeof(ubo));

    const uint32_t groups = (maxParticles + 255u) / 256u;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_.getPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_.getLayout(),
                            0, 1, &descSets_[frameIndex], 0, nullptr);
    vkCmdDispatch(cmd, groups, 1, 1);

    VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

} // namespace Phantom::Particles
