#include "VulkanComputePipeline.h"
#include "VulkanContext.h"
#include "detail/VkCheckInternal.h"

namespace Phantom::VKG {

VkShaderModule VulkanComputePipeline::createShaderModule(VkDevice device,
                                                          const std::vector<uint32_t>& spv) const
{
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spv.size() * sizeof(uint32_t);
    ci.pCode    = spv.data();

    VkShaderModule module = VK_NULL_HANDLE;
    VKG_CHECK(vkCreateShaderModule(device, &ci, nullptr, &module),
              "Failed to create compute shader module", VK_NULL_HANDLE);
    return module;
}

bool VulkanComputePipeline::create(const VulkanContext& ctx,
                                    const ComputePipelineConfig& config)
{
    VkDevice device = ctx.getDevice();

    // --- Pipeline layout ---
    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (config.descriptorSetLayout != VK_NULL_HANDLE) {
        layoutCI.setLayoutCount = 1;
        layoutCI.pSetLayouts    = &config.descriptorSetLayout;
    }

    if (config.pushConstantRange.size > 0) {
        layoutCI.pushConstantRangeCount = 1;
        layoutCI.pPushConstantRanges    = &config.pushConstantRange;
    }

    VKG_CHECK(vkCreatePipelineLayout(device, &layoutCI, nullptr, &layout_),
              "Failed to create compute pipeline layout", false);

    // --- Shader module ---
    VkShaderModule compModule = createShaderModule(device, config.compSpv);
    if (compModule == VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
        return false;
    }

    VkPipelineShaderStageCreateInfo stageCI{};
    stageCI.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCI.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stageCI.module = compModule;
    stageCI.pName  = "main";

    // --- Compute pipeline ---
    VkComputePipelineCreateInfo pipelineCI{};
    pipelineCI.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCI.stage  = stageCI;
    pipelineCI.layout = layout_;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to create compute pipeline\n");
        vkDestroyShaderModule(device, compModule, nullptr);
        vkDestroyPipelineLayout(device, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
        return false;
    }

    vkDestroyShaderModule(device, compModule, nullptr);
    return true;
}

void VulkanComputePipeline::destroy(VkDevice device)
{
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }
}

} // namespace VKG
