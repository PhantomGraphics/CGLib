#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "ParticleEmitter.h"
#include "ParticlePool.h"
#include "../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../CGLib/VulkanGraphics/VulkanComputePipeline.h"
#include "../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"

#include <vector>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::Particles {

// Drives a fixed-capacity GPU particle pool with a single compute dispatch per frame:
// ParticleEmitter decides which ring-buffer slots to (re)spawn this frame, the shader
// respawns those and integrates gravity/drag for everyone else. See
// shaders/particle_update.comp for the per-particle logic and docs/todo/PLAN_universe_app.md
// Phase B for the overall design.
class ParticleSimulator {
public:
    struct Shaders {
        std::vector<uint32_t> updateCompSpv;
    };

    ParticleSimulator() = default;
    ParticleSimulator(const ParticleSimulator&) = delete;
    ParticleSimulator& operator=(const ParticleSimulator&) = delete;

    void setShaders(Shaders s) { shaders_ = std::move(s); }

    void create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                uint32_t maxParticles, uint32_t framesInFlight);
    void destroy(VkDevice device);

    ParticleEmitter&       emitter()       { return emitter_; }
    const ParticleEmitter& emitter() const { return emitter_; }

    // Records the compute dispatch + SSBO-write -> vertex-read barrier into the caller's
    // per-frame command buffer. Must be called before the render pass that draws the
    // particles (e.g. from onUpdate(), before onRender() begins its render pass).
    void recordUpdate(VkCommandBuffer cmd, uint32_t frameIndex, float dt);

    VkBuffer getParticleBuffer() const { return pool_.getBuffer(); }
    uint32_t getMaxParticles()   const { return pool_.getMaxParticles(); }
    bool     isValid()           const { return pipeline_.isValid(); }

private:
    // std140 layout (112 bytes); field grouping matches the `SimParams` uniform block in
    // shaders/particle_update.comp exactly -- keep both in sync.
    struct SimUBO {
        glm::vec4  originDt;      // xyz = emitter origin, w = dt
        glm::vec4  gravityDrag;   // xyz = gravity, w = drag
        glm::vec4  initialColor;
        glm::vec4  endColor;
        glm::vec4  speedParams;   // x=speed, y=speedVariance, z=lifeTime, w=size
        glm::uvec4 counts;        // x=maxParticles, y=emitStart, z=emitCount, w=seed
        glm::vec4  shapeParams;   // x=shapeRadius, y=shape, z/w unused
    };
    static_assert(sizeof(SimUBO) == 112, "SimUBO layout changed -- update particle_update.comp SimParams accordingly");

    Shaders          shaders_;
    ParticlePool     pool_;
    ParticleEmitter  emitter_;

    Phantom::VKG::VulkanDescriptorSetLayout dsl_;
    Phantom::VKG::VulkanDescriptorPool      descPool_;
    std::vector<VkDescriptorSet>            descSets_;
    std::vector<Phantom::VKG::VulkanBuffer> simUBO_;
    Phantom::VKG::VulkanComputePipeline     pipeline_;

    uint32_t framesInFlight_ = 0;
};

} // namespace Phantom::Particles
