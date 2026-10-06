#include "GltfMainPipelines.h"

#include "../../VulkanGraphics/VulkanContext.h"

namespace Phantom::Gltf {

GltfFixedFunctionState gltfFixedFunctionState(bool blend, bool doubleSided, VkCullModeFlags defaultCull)
{
    GltfFixedFunctionState s;
    s.cullMode    = doubleSided ? VK_CULL_MODE_NONE : defaultCull;
    s.blendEnable = blend;
    s.depthWrite  = !blend;
    return s;
}

bool GltfMainPipelines::create(Phantom::VKG::VulkanContext& ctx, VkRenderPass renderPass,
                               Phantom::VKG::PipelineConfig base, VkCullModeFlags defaultCull)
{
    destroy(ctx.getDevice());
    if (base.vertSpv.empty() || base.fragSpv.empty()) return false;
    for (int blend = 0; blend < 2; ++blend) {
        for (int dbl = 0; dbl < 2; ++dbl) {
            const GltfFixedFunctionState s = gltfFixedFunctionState(blend != 0, dbl != 0, defaultCull);
            base.cullMode    = s.cullMode;
            base.blendEnable = s.blendEnable;
            base.depthWrite  = s.depthWrite;
            if (!select(blend != 0, dbl != 0).create(ctx, renderPass, base)) {
                destroy(ctx.getDevice());
                return false;
            }
        }
    }
    return true;
}

void GltfMainPipelines::destroy(VkDevice device)
{
    for (auto& p : pipelines_) p.destroy(device);
}

}
