#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace Phantom::VKG {

class VulkanContext;

/// @brief Aggregates all parameters needed to create a compute pipeline.
struct ComputePipelineConfig {
    /// @brief SPIR-V bytecode for the compute shader (as a uint32_t word array).
    std::vector<uint32_t> compSpv;

    /// @brief Descriptor set layout bound at set 0.  Pass VK_NULL_HANDLE if no descriptors are used.
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    /// @brief Optional push-constant range.  Set size > 0 to enable push constants.
    VkPushConstantRange pushConstantRange{};
};

/// @brief Creates and owns a VkPipeline (compute) together with its VkPipelineLayout.
///
/// Usage:
/// @code
///   VulkanComputePipeline pipeline;
///   ComputePipelineConfig cfg;
///   cfg.compSpv = { ... };
///   cfg.descriptorSetLayout = layout;
///   pipeline.create(ctx, cfg);
///
///   // Per-frame recording:
///   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getPipeline());
///   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getLayout(), ...);
///   vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
/// @endcode
class VulkanComputePipeline {
public:
    VulkanComputePipeline() = default;
    VulkanComputePipeline(const VulkanComputePipeline&) = delete;
    VulkanComputePipeline& operator=(const VulkanComputePipeline&) = delete;
    ~VulkanComputePipeline() = default;

    /// @brief Creates the pipeline layout and compute pipeline from the given configuration.
    ///
    /// The shader module is created internally and destroyed immediately after pipeline creation.
    /// @param ctx    Logical device context.
    /// @param config Pipeline configuration (see ComputePipelineConfig).
    /// @return false if shader module or pipeline creation fails.
    bool create(const VulkanContext& ctx, const ComputePipelineConfig& config);

    /// @brief Destroys the pipeline and its layout.
    /// @param device Logical device that owns the pipeline.
    void destroy(VkDevice device);

    /// @brief Returns the VkPipeline handle.
    VkPipeline getPipeline() const { return pipeline_; }

    /// @brief Returns the VkPipelineLayout handle.
    VkPipelineLayout getLayout() const { return layout_; }

    bool isValid() const { return pipeline_ != VK_NULL_HANDLE; }

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
