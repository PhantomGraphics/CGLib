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
                               Phantom::VKG::PipelineConfig base, VkCullModeFlags defaultCull, bool withWireframe)
{
    destroy(ctx.getDevice());
    wireReady_ = false;
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
    if (withWireframe) {
        base.polygonMode = VK_POLYGON_MODE_LINE;
        wireReady_ = true;
        for (int blend = 0; blend < 2 && wireReady_; ++blend) {
            for (int dbl = 0; dbl < 2 && wireReady_; ++dbl) {
                const GltfFixedFunctionState s = gltfFixedFunctionState(blend != 0, dbl != 0, defaultCull);
                base.cullMode = s.cullMode; base.blendEnable = s.blendEnable; base.depthWrite = s.depthWrite;
                wireReady_ = pipelines_[4 + blend * 2 + dbl].create(ctx, renderPass, base);
            }
        }
        if (!wireReady_) {   // the filled variants stay usable; wireframe just falls back to fill
            for (int i = 4; i < 8; ++i) pipelines_[i].destroy(ctx.getDevice());
        }
    }
    return true;
}

void GltfMainPipelines::destroy(VkDevice device)
{
    for (auto& p : pipelines_) p.destroy(device);
    wireReady_ = false;
}

}
