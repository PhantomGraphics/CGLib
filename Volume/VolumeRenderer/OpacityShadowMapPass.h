#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "../../../CGLib/VulkanGraphics/VulkanSampler.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace Phantom::VKG { class VulkanContext; }

namespace Phantom::Volume {

// Multi-layer opacity accumulation target for the PBVR self-shadow experiment
// (internal design notes).
//
// Holds a single VkImage with `layerCount` array layers (VK_FORMAT_R32_SFLOAT, no depth).
// Layer i is meant to be filled by additively blending the light-space PBVR particle set
// with a per-layer far-depth cutoff, so that layer i ends up holding "cumulative density
// from the light down to light-space depth (i+1)/layerCount". The whole array is then
// sampled as a VK_IMAGE_VIEW_TYPE_2D_ARRAY in the main pass to reconstruct transmittance.
//
// Typical usage:
//   pass.create(ctx, 512, 8);
//   pass.setLightViewProj(lightView, lightProj);
//   for (uint32_t i = 0; i < pass.getLayerCount(); ++i) {
//       pass.beginLayer(cmd, i);
//       // ... draw the PBVR particle set with the deposit pipeline ...
//       pass.endLayer(cmd);
//   }
//   // main pass samples pass.getArrayView()/getSampler() with pass.getLightVP()
class OpacityShadowMapPass {
public:
    OpacityShadowMapPass() = default;
    OpacityShadowMapPass(const OpacityShadowMapPass&) = delete;
    OpacityShadowMapPass& operator=(const OpacityShadowMapPass&) = delete;

    // size: width/height of each layer (square). layerCount must be >= 1.
    // @return false if resource creation fails; no partial state is left (destroy() will
    // still be safe to call, but create() itself already rolls back internally on failure).
    bool create(const Phantom::VKG::VulkanContext& ctx, uint32_t size, uint32_t layerCount);
    void destroy(const Phantom::VKG::VulkanContext& ctx);

    // Caller is responsible for constructing view/proj so that both this pass's deposit
    // render and the main pass's shadow-map sampling use the identical lightVP = proj*view
    // (same convention as Phantom::Gltf::ShadowMapPass).
    void setLightViewProj(const glm::mat4& view, const glm::mat4& proj) { lightVP_ = proj * view; }

    // layerIndex must be in [0, getLayerCount()). Caller must not nest with another render pass.
    void beginLayer(VkCommandBuffer cmd, uint32_t layerIndex) const;
    void endLayer(VkCommandBuffer cmd) const;

    VkRenderPass getRenderPass()  const { return renderPass_; }
    VkImageView  getArrayView()   const { return arrayView_; }   // VK_IMAGE_VIEW_TYPE_2D_ARRAY, for sampling
    VkSampler    getSampler()     const { return sampler_.get(); }
    glm::mat4    getLightVP()     const { return lightVP_; }
    uint32_t     getLayerCount()  const { return layerCount_; }
    uint32_t     getSize()        const { return size_; }
    bool         isValid()        const { return renderPass_ != VK_NULL_HANDLE; }

private:
    VkImage        image_      = VK_NULL_HANDLE;
    VkDeviceMemory memory_     = VK_NULL_HANDLE;
    VkImageView    arrayView_  = VK_NULL_HANDLE;
    std::vector<VkImageView>   layerViews_;
    std::vector<VkFramebuffer> framebuffers_;
    VkRenderPass   renderPass_ = VK_NULL_HANDLE;
    Phantom::VKG::VulkanSampler sampler_;

    glm::mat4 lightVP_    = glm::mat4(1.0f);
    uint32_t  size_       = 0;
    uint32_t  layerCount_ = 0;
};

} // namespace Phantom::Volume
