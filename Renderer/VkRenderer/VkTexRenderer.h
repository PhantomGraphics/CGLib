#pragma once

#include "IVkRenderer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanPipeline.h"

#include <vector>

namespace Phantom::VKG {

/// @brief Vulkan renderer that blits a texture to the full screen (corresponds to OpenGL TexRenderer).
///
/// No vertex buffer is needed. The vertex shader generates a full-screen quad from gl_VertexIndex,
/// so the draw call is simply `vkCmdDraw(cmd, 6, 1, 0, 0)`.
///
/// Set the source texture before each frame (or whenever it changes) via setTexture().
/// Passing `VulkanOffscreen::getColorImageView()` directly allows displaying an offscreen result.
///
/// Usage:
/// @code
///   VkTexRenderer::Config cfg;
///   cfg.vertSpv = loadSPV("tex.vert.spv");
///   cfg.fragSpv = loadSPV("tex.frag.spv");
///   texRenderer.create(ctx, pool, swapChainRenderPass);
///
///   // per-frame:
///   texRenderer.setTexture(device, offscreen.getColorImageView(), sampler.get(), frameIndex);
///   texRenderer.render(cmd, frameIndex);
/// @endcode
class VkTexRenderer : public IVkRenderer {
public:
    struct Config {
        std::vector<uint32_t> vertSpv;                          ///< SPIR-V for tex.vert.
        std::vector<uint32_t> fragSpv;                          ///< SPIR-V for tex.frag.
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT; ///< MSAA sample count (must match render pass).
    };

    explicit VkTexRenderer(Config config) : config_(std::move(config)) {}

    void create(const VulkanContext& ctx,
                const VulkanCommandPool& pool,
                VkRenderPass renderPass,
                uint32_t framesInFlight = 2) override;

    void destroy(VkDevice device) override;

    /// @brief Bind a texture image to the descriptor set for the given frame.
    ///
    /// Call before render() on each frame (or whenever the image changes).
    /// @param device      Logical device.
    /// @param imageView   Color image view to sample. Pass VulkanOffscreen::getColorImageView() directly.
    /// @param sampler     Sampler handle. Pass VulkanSampler::get().
    /// @param frameIndex  Index of the descriptor set to update (0..framesInFlight-1).
    void setTexture(VkDevice device, VkImageView imageView, VkSampler sampler, uint32_t frameIndex);

    void render(VkCommandBuffer cmd, uint32_t frameIndex) override;

    bool isValid() const override { return pipeline_.getPipeline() != VK_NULL_HANDLE; }

private:
    Config   config_;
    uint32_t framesInFlight_ = 2;

    VulkanDescriptorSetLayout     descriptorSetLayout_;
    VulkanDescriptorPool          descriptorPool_;
    std::vector<VkDescriptorSet>         descriptorSets_;

    VulkanPipeline pipeline_;

    bool hasTexture_ = false;
};

} // namespace Phantom::VKG
