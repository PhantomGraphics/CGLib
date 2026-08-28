#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "IVkRenderer.h"
#include "VkRenderBufferTypes.h"
#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanPipeline.h"

#include <array>
#include <vector>

namespace Phantom::VKG {

/// @brief Vulkan renderer for point clouds (corresponds to OpenGL PointRenderer).
///
/// Vertex data is passed through three separate bindings:
///   binding 0 - position (vec3, stride=12)
///   binding 1 - color    (vec4, stride=16)
///   binding 2 - size     (float, stride=4)
///
/// Load shaders with loadSPV() and assign to Config.vertSpv / Config.fragSpv,
/// or point to Shaders/point.vert.spv and Shaders/point.frag.spv.
class VkPointRenderer : public IVkRenderer {
public:
    /// @brief Configuration required to build the pipeline.
    struct Config {
        std::vector<uint32_t> vertSpv;                          ///< SPIR-V for point.vert.
        std::vector<uint32_t> fragSpv;                          ///< SPIR-V for point.frag.
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT; ///< MSAA sample count (must match render pass).
    };

    /// @brief Per-frame draw data.
    using Buffer = VkPointBufferData;

    explicit VkPointRenderer(Config config) : config_(std::move(config)) {}

    void create(const VulkanContext& ctx,
                const VulkanCommandPool& pool,
                VkRenderPass renderPass,
                uint32_t framesInFlight = 2) override;

    void destroy(VkDevice device) override;

    /// @brief Upload CPU buffer data to the GPU.
    ///
    /// Call before render() each frame.
    void upload(const VulkanContext& ctx,
                const VulkanCommandPool& pool,
                const Buffer& buffer);

    void render(VkCommandBuffer cmd, uint32_t frameIndex) override;

    bool isValid() const override { return pipeline_.getPipeline() != VK_NULL_HANDLE; }

private:
    struct UBOData { glm::mat4 mvp; };

    Config   config_;
    uint32_t framesInFlight_ = 2;
    uint32_t pointCount_     = 0;

    VulkanDescriptorSetLayout descriptorSetLayout_;
    VulkanDescriptorPool      descriptorPool_;
    std::vector<VkDescriptorSet> descriptorSets_;

    VulkanPipeline pipeline_;

    VulkanBuffer positionBuffer_;
    VulkanBuffer colorBuffer_;
    VulkanBuffer sizeBuffer_;

    std::vector<VulkanBuffer> uniformBuffers_; // one per frame in flight
};

} // namespace Phantom::VKG
