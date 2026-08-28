#pragma once

#include "../../CGLib/VulkanGraphics/VulkanBuffer.h"

#include <cstdint>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::Particles {

// Owns the single GPU-resident ParticleGpu[maxParticles] storage buffer shared by
// ParticleSimulator (compute read/write) and ParticleBillboardRenderer (vertex-stage read).
// Zero-initialised on creation so every slot starts dead (position.w == life == 0).
class ParticlePool {
public:
    ParticlePool() = default;
    ParticlePool(const ParticlePool&) = delete;
    ParticlePool& operator=(const ParticlePool&) = delete;

    void create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                uint32_t maxParticles);
    void destroy(VkDevice device);

    VkBuffer getBuffer()      const { return buffer_.get(); }
    uint32_t getMaxParticles() const { return maxParticles_; }
    bool     isValid()        const { return buffer_.isValid(); }

private:
    Phantom::VKG::VulkanBuffer buffer_;
    uint32_t maxParticles_ = 0;
};

} // namespace Phantom::Particles
