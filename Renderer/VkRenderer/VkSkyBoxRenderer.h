#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "IVkRenderer.h"
#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanPipeline.h"

#include <vector>

namespace Phantom::VKG {

/// @brief Renders a skybox using a cube-map texture (corresponds to OpenGL SkyBoxRenderer).
///
/// Vertex layout (binding 0): cube corner positions only (vec3, stride=12).
/// The cube has side length 2 centered at the origin.
///
/// Descriptor layout:
///   binding 0 (vertex)   - UBO  { mat4 projection; mat4 view; }
///   binding 1 (fragment) - samplerCube
///
/// Usage:
/// @code
///   VkSkyBoxRenderer::Config cfg;
///   cfg.vertSpv = loadSPV("skybox.vert.spv");
///   cfg.fragSpv = loadSPV("skybox.frag.spv");
///   skybox.create(ctx, pool, renderPass);
///
///   // Set cube-map image once (or when it changes):
///   skybox.setCubeMap(device, cubeMapView, sampler);
///
///   // Per-frame:
///   skybox.upload(ctx, pool, buffer);  // update matrices
///   skybox.render(cmd, frameIndex);
/// @endcode
class VkSkyBoxRenderer : public IVkRenderer {
public:
    struct Config {
        std::vector<uint32_t> vertSpv;                          ///< skybox.vert SPIR-V.
        std::vector<uint32_t> fragSpv;                          ///< skybox.frag SPIR-V.
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT; ///< MSAA sample count (must match render pass).
    };

    /// @brief Per-frame data (camera matrices only; geometry is fixed).
    struct Buffer {
        glm::mat4 projectionMatrix{1.f};
        /// View matrix with translation stripped (rotation only).
        glm::mat4 viewMatrix{1.f};
    };

    explicit VkSkyBoxRenderer(Config config) : config_(std::move(config)) {}

    void create(const VulkanContext& ctx,
                const VulkanCommandPool& pool,
                VkRenderPass renderPass,
                uint32_t framesInFlight = 2) override;

    void destroy(VkDevice device) override;

    /// @brief Bind the cube-map image and sampler to all descriptor sets.
    ///
    /// Call once after create(), and again whenever the cube-map changes.
    /// @param device        Logical device.
    /// @param cubeMapView   VkImageView for a VK_IMAGE_VIEW_TYPE_CUBE image.
    /// @param sampler       Sampler handle (VulkanSampler::get()).
    void setCubeMap(VkDevice device, VkImageView cubeMapView, VkSampler sampler);

    /// @brief Update the camera matrices for the given frame.
    void upload(const Buffer& buffer, uint32_t frameIndex);

    void render(VkCommandBuffer cmd, uint32_t frameIndex) override;

    bool isValid() const override { return pipeline_.getPipeline() != VK_NULL_HANDLE; }

private:
    struct UBOData {
        glm::mat4 projection;
        glm::mat4 view;
    };

    Config   config_;
    uint32_t framesInFlight_ = 2;

    VulkanDescriptorSetLayout    descriptorSetLayout_;
    VulkanDescriptorPool         descriptorPool_;
    std::vector<VkDescriptorSet>        descriptorSets_;

    VulkanPipeline pipeline_;

    VulkanBuffer positionBuffer_;
    VulkanBuffer indexBuffer_;

    std::vector<VulkanBuffer> uniformBuffers_;

    bool hasCubeMap_ = false;
};

} // namespace Phantom::VKG
