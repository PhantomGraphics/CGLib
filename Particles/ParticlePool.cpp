#include "ParticlePool.h"
#include "ParticleGpu.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <vector>

namespace Phantom::Particles {

void ParticlePool::create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                           uint32_t maxParticles)
{
    maxParticles_ = maxParticles;

    // Zero-init: position.w == 0 marks every slot dead until ParticleEmitter claims it.
    std::vector<ParticleGpu> initial(maxParticles);

    buffer_.create(ctx, pool,
                    static_cast<VkDeviceSize>(maxParticles) * sizeof(ParticleGpu),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    initial.data());
}

void ParticlePool::destroy(VkDevice device)
{
    buffer_.destroy(device);
    maxParticles_ = 0;
}

} // namespace Phantom::Particles
