#pragma once

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace Phantom::VKG { class VulkanContext; }

namespace Phantom::PostProcess::detail {

// Binding layout produced by create(): binding 0 is a per-frame UBO when uboSize > 0,
// followed by `sampledImageCount` combined-image-sampler bindings (all VK_SHADER_STAGE_FRAGMENT_BIT).
struct FullscreenEffectPipelineConfig {
    std::vector<uint32_t> vertSpv;
    std::vector<uint32_t> fragSpv;
    uint32_t framesInFlight    = 2;
    uint32_t uboSize           = 0;  // 0 = no UBO binding
    uint32_t sampledImageCount = 1;  // number of combined-image-sampler bindings

    // Fragment-stage push constant block size in bytes (0 = none). Use this instead of the
    // UBO for values that vary across multiple draw() calls recorded into the same,
    // not-yet-submitted command buffer within a single frame (e.g. Kawase blur's per-
    // iteration offset in BloomEffect) -- a descriptor set or UBO written repeatedly before
    // one vkQueueSubmit only ever reflects the LAST CPU write to every draw that references
    // it, since the GPU dereferences descriptor/buffer contents at execution time, not at
    // vkCmdBindDescriptorSets/vkCmdDraw record time. Push constants are recorded directly
    // into the command stream and don't have this hazard.
    uint32_t pushConstantSize  = 0;
};

// Builds a graphics pipeline + per-frame descriptor sets for a single fullscreen-triangle
// post-process pass (no vertex buffer -- the vertex shader derives position/UV from
// gl_VertexIndex, see shaders/passthrough.vert).
//
// A single instance can be replayed against multiple VulkanOffscreen targets (e.g. Kawase
// blur ping-pong) by calling setSampledImage() to rewrite the input(s) before each draw().
// This mirrors the pattern already used by Physics/FluidRenderer's BilateralFilter/SSFRPipeline.
class FullscreenEffectPipeline {
public:
    FullscreenEffectPipeline() = default;
    FullscreenEffectPipeline(const FullscreenEffectPipeline&) = delete;
    FullscreenEffectPipeline& operator=(const FullscreenEffectPipeline&) = delete;

    // @throws std::runtime_error if shader module, descriptor, or pipeline creation fails
    //         (matches CGLib/VulkanGraphics error-handling convention).
    void create(const Phantom::VKG::VulkanContext& ctx, VkRenderPass renderPass,
                const FullscreenEffectPipelineConfig& cfg);
    void destroy(VkDevice device);

    // Valid only when cfg.uboSize > 0.
    void updateUBO(uint32_t frame, const void* data, size_t size);

    // Writes a combined-image-sampler descriptor at binding (firstImageBinding() + slot)
    // for the given frame's descriptor set. slot must be < cfg.sampledImageCount.
    void setSampledImage(VkDevice device, uint32_t frame, uint32_t slot,
                         VkImageView view, VkSampler sampler);

    // Binds the pipeline + frame's descriptor set, optionally pushes a push-constant block
    // (pushData/pushSize must match cfg.pushConstantSize when non-null), and issues the
    // fullscreen-triangle draw (vkCmdDraw(cmd, 3, 1, 0, 0)). Caller must have already called
    // VulkanOffscreen::beginRenderPass() on the target and must call endRenderPass() after.
    void draw(VkCommandBuffer cmd, uint32_t frame,
              const void* pushData = nullptr, uint32_t pushSize = 0) const;

    bool isValid() const { return pipeline_ != VK_NULL_HANDLE; }

private:
    uint32_t framesInFlight_    = 0;
    uint32_t uboSize_           = 0;
    uint32_t sampledImageCount_ = 0;
    uint32_t firstImageBinding_ = 0; // 1 if a UBO occupies binding 0, else 0
    uint32_t pushConstantSize_  = 0;

    VkPipeline       pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_   = VK_NULL_HANDLE;

    Phantom::VKG::VulkanDescriptorSetLayout setLayout_;
    Phantom::VKG::VulkanDescriptorPool      descPool_;
    std::vector<VkDescriptorSet>            descSets_;
    std::vector<Phantom::VKG::VulkanBuffer> ubo_;

    VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& spv) const;
};

} // namespace Phantom::PostProcess::detail
