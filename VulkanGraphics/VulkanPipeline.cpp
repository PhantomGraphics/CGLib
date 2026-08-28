#include "VulkanPipeline.h"
#include "VulkanContext.h"
#include "detail/VkCheckInternal.h"

namespace Phantom::VKG {

bool VulkanPipeline::create(const VulkanContext& ctx,
                             VkRenderPass renderPass, const PipelineConfig& cfg)
{
    VkDevice device = ctx.getDevice();

    VkShaderModule vertMod = createShaderModule(device, cfg.vertSpv);
    if (vertMod == VK_NULL_HANDLE) return false;

    VkShaderModule fragMod = createShaderModule(device, cfg.fragSpv);
    if (fragMod == VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, vertMod, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount   = (uint32_t)cfg.bindingDescs.size();
    vi.pVertexBindingDescriptions      = cfg.bindingDescs.data();
    vi.vertexAttributeDescriptionCount = (uint32_t)cfg.attrDescs.size();
    vi.pVertexAttributeDescriptions    = cfg.attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = cfg.topology;

    VkPipelineViewportStateCreateInfo vs{};
    vs.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = cfg.polygonMode;
    rs.cullMode    = cfg.cullMode;
    rs.frontFace   = cfg.frontFace;
    rs.lineWidth   = cfg.lineWidth;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = cfg.samples;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = cfg.depthTest  ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = cfg.depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp   = cfg.depthCompareOp;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable    = cfg.blendEnable ? VK_TRUE : VK_FALSE;
    if (cfg.blendEnable && cfg.additiveBlend) {
        cba.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cba.colorBlendOp        = VK_BLEND_OP_ADD;
        cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba.alphaBlendOp        = VK_BLEND_OP_ADD;
    } else if (cfg.blendEnable) {
        cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cba.colorBlendOp        = VK_BLEND_OP_ADD;
        cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        cba.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    // Collect descriptor set layouts: vector takes priority over single layout.
    std::vector<VkDescriptorSetLayout> allLayouts;
    if (!cfg.descriptorSetLayouts.empty()) {
        allLayouts = cfg.descriptorSetLayouts;
    } else if (cfg.descriptorSetLayout != VK_NULL_HANDLE) {
        allLayouts.push_back(cfg.descriptorSetLayout);
    }

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (!allLayouts.empty()) {
        plci.setLayoutCount = static_cast<uint32_t>(allLayouts.size());
        plci.pSetLayouts    = allLayouts.data();
    }
    if (!cfg.pushConstantRanges.empty()) {
        plci.pushConstantRangeCount = static_cast<uint32_t>(cfg.pushConstantRanges.size());
        plci.pPushConstantRanges    = cfg.pushConstantRanges.data();
    }

    if (vkCreatePipelineLayout(device, &plci, nullptr, &layout_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to create pipeline layout\n");
        vkDestroyShaderModule(device, vertMod, nullptr);
        vkDestroyShaderModule(device, fragMod, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount          = 2;
    pci.pStages             = stages;
    pci.pVertexInputState   = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState      = &vs;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState   = &ms;
    pci.pDepthStencilState  = &ds;
    pci.pColorBlendState    = &cb;
    pci.pDynamicState       = &dyn;
    pci.layout              = layout_;
    pci.renderPass          = renderPass;
    pci.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to create graphics pipeline\n");
        vkDestroyShaderModule(device, vertMod, nullptr);
        vkDestroyShaderModule(device, fragMod, nullptr);
        vkDestroyPipelineLayout(device, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
        return false;
    }

    vkDestroyShaderModule(device, vertMod, nullptr);
    vkDestroyShaderModule(device, fragMod, nullptr);
    return true;
}

void VulkanPipeline::destroy(VkDevice device) {
    if (pipeline_) { vkDestroyPipeline(device, pipeline_, nullptr);       pipeline_ = VK_NULL_HANDLE; }
    if (layout_)   { vkDestroyPipelineLayout(device, layout_, nullptr);   layout_   = VK_NULL_HANDLE; }
}

VkShaderModule VulkanPipeline::createShaderModule(
    VkDevice device, const std::vector<uint32_t>& spv) const
{
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spv.size() * sizeof(uint32_t);
    ci.pCode    = spv.data();

    VkShaderModule mod = VK_NULL_HANDLE;
    VKG_CHECK(vkCreateShaderModule(device, &ci, nullptr, &mod),
              "Failed to create shader module", VK_NULL_HANDLE);
    return mod;
}

} // namespace VKG
