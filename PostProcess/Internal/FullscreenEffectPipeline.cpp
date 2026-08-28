#include "FullscreenEffectPipeline.h"
#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

#include <stdexcept>

namespace Phantom::PostProcess::detail {

void FullscreenEffectPipeline::create(const Phantom::VKG::VulkanContext& ctx, VkRenderPass renderPass,
                                       const FullscreenEffectPipelineConfig& cfg)
{
    framesInFlight_     = cfg.framesInFlight;
    uboSize_            = cfg.uboSize;
    sampledImageCount_  = cfg.sampledImageCount;
    firstImageBinding_  = (uboSize_ > 0) ? 1u : 0u;
    pushConstantSize_   = cfg.pushConstantSize;
    VkDevice device     = ctx.getDevice();

    // --- Descriptor set layout: [UBO?] + N combined-image-samplers ---
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    if (uboSize_ > 0) {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = 0;
        b.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(b);
    }
    for (uint32_t i = 0; i < sampledImageCount_; ++i) {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = firstImageBinding_ + i;
        b.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = 1;
        b.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(b);
    }
    setLayout_.create(device, bindings);

    // --- Shader modules ---
    VkShaderModule vertMod = createShaderModule(device, cfg.vertSpv);
    VkShaderModule fragMod = createShaderModule(device, cfg.fragSpv);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    // --- Vertex input: none -- passthrough.vert derives position/UV from gl_VertexIndex ---
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vs{};
    vs.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode    = VK_CULL_MODE_NONE;
    rs.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Post-process passes never test/write depth -- the render pass they run in only
    // carries a depth attachment because VulkanOffscreen requires one.
    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = VK_FALSE;
    ds.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    VkDescriptorSetLayout descLayout = setLayout_.get();
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset     = 0;
    pushRange.size       = pushConstantSize_;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount         = 1;
    plci.pSetLayouts            = &descLayout;
    plci.pushConstantRangeCount = pushConstantSize_ > 0 ? 1u : 0u;
    plci.pPushConstantRanges    = pushConstantSize_ > 0 ? &pushRange : nullptr;

    if (vkCreatePipelineLayout(device, &plci, nullptr, &layout_) != VK_SUCCESS)
        throw std::runtime_error("[PostProcess] Failed to create pipeline layout");

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

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_) != VK_SUCCESS)
        throw std::runtime_error("[PostProcess] Failed to create graphics pipeline");

    vkDestroyShaderModule(device, vertMod, nullptr);
    vkDestroyShaderModule(device, fragMod, nullptr);

    // --- Per-frame UBO ---
    if (uboSize_ > 0) {
        ubo_.resize(framesInFlight_);
        for (auto& ub : ubo_)
            ub.createMapped(ctx, uboSize_, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    // --- Per-frame descriptor sets ---
    std::vector<VkDescriptorPoolSize> poolSizes;
    if (uboSize_ > 0)
        poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight_ });
    if (sampledImageCount_ > 0)
        poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, framesInFlight_ * sampledImageCount_ });
    descPool_.create(device, poolSizes, framesInFlight_);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight_, descLayout);
    descSets_ = descPool_.allocateSets(device, layouts);

    if (uboSize_ > 0) {
        for (uint32_t i = 0; i < framesInFlight_; ++i) {
            VkDescriptorBufferInfo bi{};
            bi.buffer = ubo_[i].get();
            bi.offset = 0;
            bi.range  = uboSize_;

            VkWriteDescriptorSet w{};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = descSets_[i];
            w.dstBinding      = 0;
            w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.descriptorCount = 1;
            w.pBufferInfo     = &bi;
            vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
        }
    }
}

void FullscreenEffectPipeline::destroy(VkDevice device)
{
    for (auto& ub : ubo_) ub.destroy(device);
    ubo_.clear();

    if (!descSets_.empty()) {
        descPool_.destroy(device);
        descSets_.clear();
    }
    setLayout_.destroy(device);

    if (pipeline_) { vkDestroyPipeline(device, pipeline_, nullptr);     pipeline_ = VK_NULL_HANDLE; }
    if (layout_)   { vkDestroyPipelineLayout(device, layout_, nullptr); layout_   = VK_NULL_HANDLE; }

    framesInFlight_ = 0;
    uboSize_ = 0;
    sampledImageCount_ = 0;
    pushConstantSize_ = 0;
}

void FullscreenEffectPipeline::updateUBO(uint32_t frame, const void* data, size_t size)
{
    ubo_[frame].write(data, static_cast<VkDeviceSize>(size));
}

void FullscreenEffectPipeline::setSampledImage(VkDevice device, uint32_t frame, uint32_t slot,
                                               VkImageView view, VkSampler sampler)
{
    VkDescriptorImageInfo ii{};
    ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ii.imageView   = view;
    ii.sampler     = sampler;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = descSets_[frame];
    w.dstBinding      = firstImageBinding_ + slot;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.descriptorCount = 1;
    w.pImageInfo      = &ii;
    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
}

void FullscreenEffectPipeline::draw(VkCommandBuffer cmd, uint32_t frame,
                                    const void* pushData, uint32_t pushSize) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    VkDescriptorSet ds = descSets_[frame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &ds, 0, nullptr);
    if (pushData != nullptr && pushSize > 0)
        vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushSize, pushData);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

VkShaderModule FullscreenEffectPipeline::createShaderModule(VkDevice device, const std::vector<uint32_t>& spv) const
{
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spv.size() * sizeof(uint32_t);
    ci.pCode    = spv.data();

    VkShaderModule mod;
    if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("[PostProcess] Failed to create shader module");
    return mod;
}

} // namespace Phantom::PostProcess::detail
