#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace Phantom::VKG {

class VulkanContext;

/// @brief Aggregates all parameters needed to create a graphics pipeline.
///
/// Zero-initialise or use aggregate initialisation, then set only the fields
/// that differ from the defaults.
struct PipelineConfig {
    /// @brief SPIR-V bytecode for the vertex shader (as a uint32_t word array).
    std::vector<uint32_t> vertSpv;

    /// @brief SPIR-V bytecode for the fragment shader (as a uint32_t word array).
    std::vector<uint32_t> fragSpv;

    /// @brief Vertex buffer binding descriptions.
    std::vector<VkVertexInputBindingDescription> bindingDescs;

    /// @brief Vertex attribute descriptions (location, format, offset).
    std::vector<VkVertexInputAttributeDescription> attrDescs;

    /// @brief Primitive assembly topology. Defaults to VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST.
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    /// @brief Descriptor set layout bound at set 0.  Pass VK_NULL_HANDLE if no descriptors are used.
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    /// @brief Multiple descriptor set layouts (set 0, set 1, ...).
    /// When non-empty, takes priority over descriptorSetLayout.
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    /// @brief Back-face culling mode. Defaults to VK_CULL_MODE_BACK_BIT.
    VkCullModeFlags cullMode  = VK_CULL_MODE_BACK_BIT;

    /// @brief Front-face winding order. Defaults to VK_FRONT_FACE_COUNTER_CLOCKWISE.
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    /// @brief Polygon fill mode. Set to VK_POLYGON_MODE_LINE for wireframe (requires fillModeNonSolid feature).
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;

    /// @brief Rasterizer line width (used when topology is LINE_LIST/STRIP). Defaults to 1.0.
    float lineWidth = 1.0f;

    /// @brief Enables depth testing. Defaults to true.
    bool depthTest  = true;

    /// @brief Enables depth writes. Defaults to true.
    bool depthWrite = true;

    /// @brief Depth comparison operator. Defaults to VK_COMPARE_OP_LESS.
    VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

    /// @brief Enables standard src-alpha / one-minus-src-alpha blending. Defaults to false.
    bool blendEnable = false;

    /// @brief When blendEnable is true, selects ADD/ONE/ONE (additive) instead of the default
    ///        src-alpha / one-minus-src-alpha blend. Ignored when blendEnable is false.
    bool additiveBlend = false;

    /// @brief MSAA sample count.  Must match the sample count of the render pass.
    ///        Defaults to VK_SAMPLE_COUNT_1_BIT (no MSAA).
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    /// @brief Push constant ranges exposed through the pipeline layout. Defaults to empty.
    std::vector<VkPushConstantRange> pushConstantRanges;
};

/// @brief Creates and owns a VkPipeline together with its VkPipelineLayout.
///
/// The viewport and scissor are set as dynamic state, so they do not need to be
/// baked into the pipeline and can be changed each frame via vkCmdSetViewport /
/// vkCmdSetScissor.
///
/// Usage:
/// @code
///   VulkanPipeline pipeline;
///   PipelineConfig cfg;
///   cfg.vertSpv = { ... };
///   cfg.fragSpv = { ... };
///   cfg.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
///   pipeline.create(ctx, renderPass, cfg);
///
///   // Per-frame recording:
///   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getPipeline());
///   vkCmdBindDescriptorSets(cmd, ..., pipeline.getLayout(), ...);
/// @endcode
class VulkanPipeline {
public:
    VulkanPipeline() = default;
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;
    ~VulkanPipeline() = default;

    /// @brief Creates the pipeline layout and graphics pipeline from the given configuration.
    ///
    /// Shader modules are created internally and destroyed immediately after pipeline creation.
    /// @param ctx        Logical device context.
    /// @param renderPass Render pass the pipeline will be used with.
    /// @param config     Pipeline configuration (see PipelineConfig).
    /// @return false if shader module or pipeline creation fails.
    bool create(const VulkanContext& ctx, VkRenderPass renderPass, const PipelineConfig& config);

    /// @brief Destroys the pipeline and its layout.
    /// @param device Logical device that owns the pipeline.
    void destroy(VkDevice device);

    /// @brief Returns the VkPipeline handle.
    VkPipeline getPipeline() const { return pipeline_; }

    /// @brief Returns the VkPipelineLayout handle.
    VkPipelineLayout getLayout() const { return layout_; }

private:
    VkPipeline       pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_   = VK_NULL_HANDLE;

    VkShaderModule createShaderModule(VkDevice device,
                                      const std::vector<uint32_t>& spv) const;
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
