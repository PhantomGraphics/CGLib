#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "../../../CGLib/VulkanGraphics/VulkanOffscreen.h"
#include "../../../CGLib/VulkanGraphics/VulkanSampler.h"

#include <cstdint>

namespace Phantom::VKG { class VulkanContext; }

namespace Phantom::Gltf {

// Depth-only offscreen pass for a single shadow-casting light. Typical usage:
//
//   shadowPass.setLightViewProj(lightView, lightProj);
//   shadowPass.begin(cmd);
//   gltfRenderer.renderShadowCasters(cmd, shadowPass.getLightVP());
//   shadowPass.end(cmd);
//   gltfRenderer.setShadowMap(shadowPass.getDepthView(), shadowPass.getShadowSampler(), shadowPass.getLightVP());
//   ... main scene render pass, where GltfSceneRenderer samples the shadow map ...
//
// Reuses VulkanOffscreen for the depth target. VulkanOffscreen always pairs a depth
// attachment with a color attachment; the color attachment here is a throwaway 1-channel
// image that is never read -- only the VK_FORMAT_D32_SFLOAT depth image (created with
// VK_IMAGE_USAGE_SAMPLED_BIT) is actually sampled from by the main pass.
class ShadowMapPass {
public:
    ShadowMapPass() = default;
    ShadowMapPass(const ShadowMapPass&) = delete;
    ShadowMapPass& operator=(const ShadowMapPass&) = delete;

    void create(const Phantom::VKG::VulkanContext& ctx, uint32_t shadowMapSize = 2048);
    void destroy(const Phantom::VKG::VulkanContext& ctx);

    // Caller is responsible for constructing view/proj so that both this pass's depth
    // render and GltfSceneRenderer's shadow-map sampling use the identical lightVP = proj*view
    // -- correctness only depends on that consistency, not on any particular Y-flip convention.
    void setLightViewProj(const glm::mat4& view, const glm::mat4& proj) { lightVP_ = proj * view; }

    void begin(VkCommandBuffer cmd) const { offscreen_.beginRenderPass(cmd); }
    void end(VkCommandBuffer cmd)   const { offscreen_.endRenderPass(cmd); }

    VkRenderPass getRenderPass()    const { return offscreen_.getRenderPass(); }
    VkImageView  getDepthView()     const { return offscreen_.getDepthImageView(); }
    VkSampler    getShadowSampler() const { return sampler_.get(); }
    glm::mat4    getLightVP()       const { return lightVP_; }
    uint32_t     getShadowMapSize() const { return size_; }
    bool         isValid()          const { return offscreen_.isValid(); }

private:
    Phantom::VKG::VulkanOffscreen offscreen_;
    Phantom::VKG::VulkanSampler   sampler_;
    glm::mat4 lightVP_ = glm::mat4(1.f);
    uint32_t  size_    = 2048;
};

} // namespace Phantom::Gltf
