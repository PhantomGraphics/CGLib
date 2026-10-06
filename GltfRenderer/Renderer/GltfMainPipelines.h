#pragma once

#include <vulkan/vulkan.h>

#include "../../VulkanGraphics/VulkanPipeline.h"

namespace Phantom::Gltf {

    // Fixed-function state of a glTF main-pass pipeline, derived from a material's two
    // independent properties (alpha BLEND, double-sided). alpha MASK is "not blend": the
    // fragment shader discards below cutoff instead.
    struct GltfFixedFunctionState {
        VkCullModeFlags cullMode    = VK_CULL_MODE_BACK_BIT;
        bool            blendEnable = false;
        bool            depthWrite  = true;
        bool operator==(const GltfFixedFunctionState& o) const {
            return cullMode == o.cullMode && blendEnable == o.blendEnable && depthWrite == o.depthWrite;
        }
    };

    // Opaque/MASK: blend off, depth write on. BLEND: src-alpha/one-minus-src-alpha, depth test on
    // but depth write off (draw order resolves overlap). Double-sided: no culling; otherwise
    // `defaultCull`.
    GltfFixedFunctionState gltfFixedFunctionState(bool blend, bool doubleSided, VkCullModeFlags defaultCull);

    // The 4 shared main-pass pipelines (blend x double-sided). All share one layout, so the
    // renderer can bind descriptor sets / push constants through layout() before choosing which
    // variant draws. create() on a live object rebuilds (destroys the old ones first).
    class GltfMainPipelines {
    public:
        // `base` carries shaders, vertex layout, descriptor layouts and push constants; its
        // cullMode/blendEnable/depthWrite are overridden per variant.
        bool create(Phantom::VKG::VulkanContext& ctx, VkRenderPass renderPass,
                    Phantom::VKG::PipelineConfig base, VkCullModeFlags defaultCull);
        void destroy(VkDevice device);

        Phantom::VKG::VulkanPipeline& select(bool blend, bool doubleSided) {
            return pipelines_[(blend ? 2 : 0) + (doubleSided ? 1 : 0)];
        }
        VkPipelineLayout layout() { return pipelines_[0].getLayout(); }

    private:
        // index = blend*2 + doubleSided: opaque, opaque 2-sided, blend, blend 2-sided
        Phantom::VKG::VulkanPipeline pipelines_[4];
    };

}
