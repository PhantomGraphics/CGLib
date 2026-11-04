#include "ParticlePool.h"
#include "ParticleGpu.h"
#include "CGLib/VulkanGraphics/VulkanContext.h"
#include "CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <vector>

namespace Phantom::Effects::Particles {

bool ParticlePool::create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                           uint32_t maxParticles)
{
    if (!ctx.getDevice() || !maxParticles || maxParticles > 1000000) return false;
    if (isValid()) return false;
    maxParticles_ = maxParticles;

    // Zero-init: position.w == 0 marks every slot dead until ParticleEmitter claims it.
    std::vector<ParticleGpu> initial(maxParticles);

    if (!buffer_.create(ctx, pool,
                    static_cast<VkDeviceSize>(maxParticles) * sizeof(ParticleGpu),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    initial.data())) { destroy(ctx.getDevice()); return false; }
    return true;
}

void ParticlePool::destroy(VkDevice device)
{
    buffer_.destroy(device);
    maxParticles_ = 0;
}

} // namespace Phantom::Effects::Particles
