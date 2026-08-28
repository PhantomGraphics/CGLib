#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "IVkRenderer.h"
#include "VkRenderBufferTypes.h"
#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanPipeline.h"

#include <vector>

namespace Phantom::VKG {

/// @brief Vulkan renderer for line segments (corresponds to OpenGL LineRenderer).
///
/// Vertex data is passed through two separate bindings:
///   binding 0 - position (vec3, stride=12)
///   binding 1 - color    (vec4, stride=16)
///
/// Draws indexed line segments using VK_PRIMITIVE_TOPOLOGY_LINE_LIST.
class VkLineRenderer : public IVkRenderer {
public:
    struct Config {
        std::vector<uint32_t> vertSpv;                          ///< SPIR-V for line.vert.
        std::vector<uint32_t> fragSpv;                          ///< SPIR-V for line.frag.
        float lineWidth = 1.0f;                                 ///< Line width (requires VkPhysicalDeviceFeatures.wideLines).
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT; ///< MSAA sample count (must match render pass).
    };

    using Buffer = VkLineBufferData;

    explicit VkLineRenderer(Config config) : config_(std::move(config)) {}

    void create(const VulkanContext& ctx,
                const VulkanCommandPool& pool,
                VkRenderPass renderPass,
                uint32_t framesInFlight = 2) override;

    void destroy(VkDevice device) override;

    void upload(const VulkanContext& ctx,
                const VulkanCommandPool& pool,
                const Buffer& buffer);

    void updateMVP(uint32_t frameIndex, const glm::mat4& mvp);

    void render(VkCommandBuffer cmd, uint32_t frameIndex) override;

    bool isValid() const override { return pipeline_.getPipeline() != VK_NULL_HANDLE; }

private:
    struct UBOData { glm::mat4 mvp; };

    Config   config_;
    uint32_t framesInFlight_ = 2;
    uint32_t indexCount_     = 0;

    VulkanDescriptorSetLayout descriptorSetLayout_;
    VulkanDescriptorPool      descriptorPool_;
    std::vector<VkDescriptorSet> descriptorSets_;

    VulkanPipeline pipeline_;

    VulkanBuffer positionBuffer_;
    VulkanBuffer colorBuffer_;
    VulkanBuffer indexBuffer_;

    std::vector<VulkanBuffer> uniformBuffers_;
};

} // namespace Phantom::VKG
