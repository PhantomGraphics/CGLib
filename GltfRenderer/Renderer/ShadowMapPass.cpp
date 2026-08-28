#include "ShadowMapPass.h"
#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

namespace Phantom::Gltf {

void ShadowMapPass::create(const Phantom::VKG::VulkanContext& ctx, uint32_t shadowMapSize)
{
    size_ = shadowMapSize;

    // R8_UNORM color attachment is required by VulkanOffscreen but never read; only the
    // D32_SFLOAT depth attachment (sampled) matters here.
    offscreen_.create(ctx, size_, size_, VK_FORMAT_R8_UNORM, VK_FORMAT_D32_SFLOAT);
    sampler_.create(ctx.getDevice(), VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

void ShadowMapPass::destroy(const Phantom::VKG::VulkanContext& ctx)
{
    sampler_.destroy(ctx.getDevice());
    offscreen_.destroy(ctx);
    size_ = 0;
}

} // namespace Phantom::Gltf
