#include "GltfIBLPrecomputer.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanImage.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdio>
#include <cstring>

using namespace Phantom::Gltf;

// Logs to stderr and returns failRet instead of throwing (this module must not use
// exceptions -- see docs/guide/conventions.md). Mirrors CGLib/VulkanGraphics's VKG_CHECK.
#define GLTF_IBL_CHECK(expr, msg, failRet) \
    do { \
        if ((expr) != VK_SUCCESS) { \
            std::fprintf(stderr, "[GltfIBLPrecomputer] %s\n", (msg)); \
            return (failRet); \
        } \
    } while (false)

namespace {

static const glm::mat4 kFaceViews[6] = {
    glm::lookAt(glm::vec3(0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
};

} // namespace

// ---------------------------------------------------------------------------
// Cube vertex data
// ---------------------------------------------------------------------------

static const float kCubeVerts[] = {
    -1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,  // -Z
    -1,-1, 1, -1, 1, 1,  1, 1, 1,  1,-1, 1,  // +Z
    -1, 1, 1, -1, 1,-1,  1, 1,-1,  1, 1, 1,  // +Y
    -1,-1,-1, -1,-1, 1,  1,-1, 1,  1,-1,-1,  // -Y
     1,-1,-1,  1,-1, 1,  1, 1, 1,  1, 1,-1,  // +X
    -1,-1, 1, -1,-1,-1, -1, 1,-1, -1, 1, 1,  // -X
};
static const uint16_t kCubeIdx[] = {
    0,1,2, 2,3,0,  4,5,6, 6,7,4,  8,9,10, 10,11,8,
    12,13,14, 14,15,12,  16,17,18, 18,19,16,  20,21,22, 22,23,20,
};

bool GltfIBLPrecomputer::createCubeBuffers(const Phantom::VKG::VulkanContext& ctx,
                                            const Phantom::VKG::VulkanCommandPool& pool)
{
    VkDevice dev = ctx.getDevice();

    // Create VB
    {
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size  = sizeof(kCubeVerts);
        bi.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        GLTF_IBL_CHECK(vkCreateBuffer(dev, &bi, nullptr, &cubeVB_), "failed to create cube VB", false);

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(dev, cubeVB_, &mr);
        auto memType = ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!memType) {
            std::fprintf(stderr, "[GltfIBLPrecomputer] no suitable memory type\n");
            return false;
        }

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = *memType;
        GLTF_IBL_CHECK(vkAllocateMemory(dev, &ai, nullptr, &cubeVBMem_), "failed to allocate cube VB memory", false);
        GLTF_IBL_CHECK(vkBindBufferMemory(dev, cubeVB_, cubeVBMem_, 0), "failed to bind cube VB memory", false);
    }

    // Upload via staging
    {
        VkBuffer stageBuf;
        VkDeviceMemory stageMem;
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size  = sizeof(kCubeVerts);
        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        GLTF_IBL_CHECK(vkCreateBuffer(dev, &bi, nullptr, &stageBuf), "failed to create staging buffer", false);

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(dev, stageBuf, &mr);
        auto stageMemType = ctx.findMemoryType(mr.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!stageMemType) {
            std::fprintf(stderr, "[GltfIBLPrecomputer] no suitable memory type\n");
            return false;
        }

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = *stageMemType;
        GLTF_IBL_CHECK(vkAllocateMemory(dev, &ai, nullptr, &stageMem), "failed to allocate staging memory", false);
        GLTF_IBL_CHECK(vkBindBufferMemory(dev, stageBuf, stageMem, 0), "failed to bind staging memory", false);

        void* mapped;
        GLTF_IBL_CHECK(vkMapMemory(dev, stageMem, 0, sizeof(kCubeVerts), 0, &mapped), "failed to map staging memory", false);
        memcpy(mapped, kCubeVerts, sizeof(kCubeVerts));
        vkUnmapMemory(dev, stageMem);

        VkCommandBuffer cmd = pool.beginSingleTimeCommands();
        VkBufferCopy copy{0, 0, sizeof(kCubeVerts)};
        vkCmdCopyBuffer(cmd, stageBuf, cubeVB_, 1, &copy);
        pool.endSingleTimeCommands(cmd);

        vkDestroyBuffer(dev, stageBuf, nullptr);
        vkFreeMemory(dev, stageMem, nullptr);
    }

    // Create IB
    {
        VkBuffer stageBuf;
        VkDeviceMemory stageMem;
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size  = sizeof(kCubeIdx);
        bi.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        GLTF_IBL_CHECK(vkCreateBuffer(dev, &bi, nullptr, &cubeIB_), "failed to create cube IB", false);

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(dev, cubeIB_, &mr);
        auto ibMemType = ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!ibMemType) {
            std::fprintf(stderr, "[GltfIBLPrecomputer] no suitable memory type\n");
            return false;
        }

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = *ibMemType;
        GLTF_IBL_CHECK(vkAllocateMemory(dev, &ai, nullptr, &cubeIBMem_), "failed to allocate cube IB memory", false);
        GLTF_IBL_CHECK(vkBindBufferMemory(dev, cubeIB_, cubeIBMem_, 0), "failed to bind cube IB memory", false);

        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        GLTF_IBL_CHECK(vkCreateBuffer(dev, &bi, nullptr, &stageBuf), "failed to create staging buffer", false);
        vkGetBufferMemoryRequirements(dev, stageBuf, &mr);
        auto ibStageMemType = ctx.findMemoryType(mr.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!ibStageMemType) {
            std::fprintf(stderr, "[GltfIBLPrecomputer] no suitable memory type\n");
            return false;
        }
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = *ibStageMemType;
        GLTF_IBL_CHECK(vkAllocateMemory(dev, &ai, nullptr, &stageMem), "failed to allocate staging memory", false);
        GLTF_IBL_CHECK(vkBindBufferMemory(dev, stageBuf, stageMem, 0), "failed to bind staging memory", false);

        void* mapped;
        GLTF_IBL_CHECK(vkMapMemory(dev, stageMem, 0, sizeof(kCubeIdx), 0, &mapped), "failed to map staging memory", false);
        memcpy(mapped, kCubeIdx, sizeof(kCubeIdx));
        vkUnmapMemory(dev, stageMem);

        VkCommandBuffer cmd = pool.beginSingleTimeCommands();
        VkBufferCopy copy{0, 0, sizeof(kCubeIdx)};
        vkCmdCopyBuffer(cmd, stageBuf, cubeIB_, 1, &copy);
        pool.endSingleTimeCommands(cmd);

        vkDestroyBuffer(dev, stageBuf, nullptr);
        vkFreeMemory(dev, stageMem, nullptr);
    }
    return true;
}

void GltfIBLPrecomputer::destroyCubeBuffers(VkDevice device)
{
    if (cubeIB_)    { vkDestroyBuffer(device, cubeIB_, nullptr);    cubeIB_    = VK_NULL_HANDLE; }
    if (cubeIBMem_) { vkFreeMemory(device, cubeIBMem_, nullptr);    cubeIBMem_ = VK_NULL_HANDLE; }
    if (cubeVB_)    { vkDestroyBuffer(device, cubeVB_, nullptr);    cubeVB_    = VK_NULL_HANDLE; }
    if (cubeVBMem_) { vkFreeMemory(device, cubeVBMem_, nullptr);    cubeVBMem_ = VK_NULL_HANDLE; }
}

// ---------------------------------------------------------------------------
// Image helpers
// ---------------------------------------------------------------------------

bool GltfIBLPrecomputer::createCubeImage(const Phantom::VKG::VulkanContext& ctx,
                                          uint32_t size, uint32_t mipLevels, VkFormat fmt,
                                          VkImage& img, VkDeviceMemory& mem)
{
    VkDevice dev = ctx.getDevice();
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.format        = fmt;
    ci.extent        = { size, size, 1 };
    ci.mipLevels     = mipLevels;
    ci.arrayLayers   = 6;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    GLTF_IBL_CHECK(vkCreateImage(dev, &ci, nullptr, &img), "failed to create cube image", false);

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(dev, img, &mr);
    auto memType = ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memType) {
        std::fprintf(stderr, "[GltfIBLPrecomputer] no suitable memory type\n");
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = *memType;
    GLTF_IBL_CHECK(vkAllocateMemory(dev, &ai, nullptr, &mem), "failed to allocate cube image memory", false);
    GLTF_IBL_CHECK(vkBindImageMemory(dev, img, mem, 0), "failed to bind cube image memory", false);
    return true;
}

VkImageView GltfIBLPrecomputer::createCubeView(VkDevice dev, VkImage img, VkFormat fmt, uint32_t mipLevels)
{
    VkImageViewCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image    = img;
    ci.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    ci.format   = fmt;
    ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.baseMipLevel   = 0;
    ci.subresourceRange.levelCount     = mipLevels;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount     = 6;

    VkImageView view;
    GLTF_IBL_CHECK(vkCreateImageView(dev, &ci, nullptr, &view), "failed to create cube image view", VK_NULL_HANDLE);
    return view;
}

VkImageView GltfIBLPrecomputer::createCubeFaceView(VkDevice dev, VkImage img, VkFormat fmt,
                                                    uint32_t face, uint32_t mip)
{
    VkImageViewCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image    = img;
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format   = fmt;
    ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.baseMipLevel   = mip;
    ci.subresourceRange.levelCount     = 1;
    ci.subresourceRange.baseArrayLayer = face;
    ci.subresourceRange.layerCount     = 1;

    VkImageView view;
    GLTF_IBL_CHECK(vkCreateImageView(dev, &ci, nullptr, &view), "failed to create cube face view", VK_NULL_HANDLE);
    return view;
}

VkSampler GltfIBLPrecomputer::createLinearSampler(VkDevice dev, uint32_t mipLevels)
{
    VkSamplerCreateInfo si{};
    si.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter        = VK_FILTER_LINEAR;
    si.minFilter        = VK_FILTER_LINEAR;
    si.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod           = static_cast<float>(mipLevels);

    VkSampler sampler;
    GLTF_IBL_CHECK(vkCreateSampler(dev, &si, nullptr, &sampler), "failed to create sampler", VK_NULL_HANDLE);
    return sampler;
}

VkShaderModule GltfIBLPrecomputer::createShaderModule(VkDevice dev, const std::vector<uint32_t>& spv)
{
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spv.size() * sizeof(uint32_t);
    ci.pCode    = spv.data();
    VkShaderModule mod;
    GLTF_IBL_CHECK(vkCreateShaderModule(dev, &ci, nullptr, &mod), "failed to create shader module", VK_NULL_HANDLE);
    return mod;
}

// ---------------------------------------------------------------------------
// Generic CubeFacePass
// ---------------------------------------------------------------------------

GltfIBLPrecomputer::CubeFacePass GltfIBLPrecomputer::createCubeFacePass(
    const Phantom::VKG::VulkanContext& ctx,
    VkFormat targetFmt,
    const std::vector<uint32_t>& vertSpv,
    const std::vector<uint32_t>& fragSpv,
    bool /*hasCubeSampler*/,
    uint32_t pushSize)
{
    VkDevice dev = ctx.getDevice();
    CubeFacePass p;

    // Render pass
    {
        VkAttachmentDescription att{};
        att.format         = targetFmt;
        att.samples        = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &ref;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        ci.attachmentCount = 1;
        ci.pAttachments    = &att;
        ci.subpassCount    = 1;
        ci.pSubpasses      = &subpass;
        ci.dependencyCount = 1;
        ci.pDependencies   = &dep;
        GLTF_IBL_CHECK(vkCreateRenderPass(dev, &ci, nullptr, &p.renderPass), "failed to create cube face render pass", CubeFacePass{});
    }

    // Descriptor set layout (binding 0: cubemap sampler)
    {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = 0;
        b.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = 1;
        b.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = 1;
        ci.pBindings    = &b;
        GLTF_IBL_CHECK(vkCreateDescriptorSetLayout(dev, &ci, nullptr, &p.dsl), "failed to create descriptor set layout", CubeFacePass{});

        VkDescriptorPoolSize ps{};
        ps.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ps.descriptorCount = 1;
        VkDescriptorPoolCreateInfo pi{};
        pi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.poolSizeCount = 1;
        pi.pPoolSizes    = &ps;
        pi.maxSets       = 1;
        GLTF_IBL_CHECK(vkCreateDescriptorPool(dev, &pi, nullptr, &p.pool_), "failed to create descriptor pool", CubeFacePass{});

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = p.pool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &p.dsl;
        GLTF_IBL_CHECK(vkAllocateDescriptorSets(dev, &ai, &p.set), "failed to allocate descriptor set", CubeFacePass{});
    }

    // Pipeline layout
    {
        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset     = 0;
        pcr.size       = pushSize;

        VkPipelineLayoutCreateInfo ci{};
        ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount         = 1;
        ci.pSetLayouts            = &p.dsl;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges    = &pcr;
        GLTF_IBL_CHECK(vkCreatePipelineLayout(dev, &ci, nullptr, &p.layout), "failed to create pipeline layout", CubeFacePass{});
    }

    // Pipeline
    {
        VkShaderModule vertMod = createShaderModule(dev, vertSpv);
        VkShaderModule fragMod = createShaderModule(dev, fragSpv);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                      VK_SHADER_STAGE_VERTEX_BIT, vertMod, "main" };
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                      VK_SHADER_STAGE_FRAGMENT_BIT, fragMod, "main" };

        VkVertexInputBindingDescription vib{0, 3 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription via{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount   = 1;
        vi.pVertexBindingDescriptions      = &vib;
        vi.vertexAttributeDescriptionCount = 1;
        vi.pVertexAttributeDescriptions    = &via;

        VkPipelineInputAssemblyStateCreateInfo ia{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };

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

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable  = VK_FALSE;
        ds.depthWriteEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = 0xF;
        cba.blendEnable    = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments    = &cba;

        VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates    = dynStates;

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
        pci.layout              = p.layout;
        pci.renderPass          = p.renderPass;
        pci.subpass             = 0;

        GLTF_IBL_CHECK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pci, nullptr, &p.pipeline), "failed to create graphics pipeline", CubeFacePass{});
        vkDestroyShaderModule(dev, vertMod, nullptr);
        vkDestroyShaderModule(dev, fragMod, nullptr);
    }

    return p;
}

void GltfIBLPrecomputer::destroyCubeFacePass(VkDevice dev, CubeFacePass& p)
{
    if (p.pipeline)   { vkDestroyPipeline(dev, p.pipeline, nullptr);             p.pipeline   = VK_NULL_HANDLE; }
    if (p.layout)     { vkDestroyPipelineLayout(dev, p.layout, nullptr);         p.layout     = VK_NULL_HANDLE; }
    if (p.pool_)      { vkDestroyDescriptorPool(dev, p.pool_, nullptr);           p.pool_      = VK_NULL_HANDLE; }
    if (p.dsl)        { vkDestroyDescriptorSetLayout(dev, p.dsl, nullptr);        p.dsl        = VK_NULL_HANDLE; }
    if (p.renderPass) { vkDestroyRenderPass(dev, p.renderPass, nullptr);          p.renderPass = VK_NULL_HANDLE; }
}

// ---------------------------------------------------------------------------
// renderCubeFaces -- renders all 6 faces at the given mip level
// ---------------------------------------------------------------------------

struct CubeFacePushBase { glm::mat4 view; glm::mat4 proj; };  // 128 bytes for irradiance passes
struct PrefilterPush    { glm::mat4 view; float roughness; float _pad[3]; };  // 80 bytes; proj is hardcoded in prefilter.vert

void GltfIBLPrecomputer::renderCubeFaces(const Phantom::VKG::VulkanContext& ctx,
                                          const Phantom::VKG::VulkanCommandPool& pool,
                                          const CubeFacePass& pass,
                                          VkImage targetImage,
                                          uint32_t faceSize,
                                          uint32_t mipLevel,
                                          float roughness)
{
    VkDevice dev = ctx.getDevice();
    static const glm::mat4 kProj =
        glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    uint32_t mipSize = faceSize >> mipLevel;
    if (mipSize == 0) mipSize = 1;

    for (uint32_t face = 0; face < 6; ++face) {
        VkImageView faceView = createCubeFaceView(dev, targetImage,
                                                  VK_FORMAT_R16G16B16A16_SFLOAT,
                                                  face, mipLevel);

        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = pass.renderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = &faceView;
        fbci.width           = mipSize;
        fbci.height          = mipSize;
        fbci.layers          = 1;
        VkFramebuffer fb;
        if (vkCreateFramebuffer(dev, &fbci, nullptr, &fb) != VK_SUCCESS) {
            std::fprintf(stderr, "[GltfIBLPrecomputer] failed to create cube face framebuffer (face %u)\n", face);
            vkDestroyImageView(dev, faceView, nullptr);
            continue;
        }

        VkCommandBuffer cmd = pool.beginSingleTimeCommands();

        VkClearValue clear{};
        clear.color = {0, 0, 0, 1};
        VkRenderPassBeginInfo rpbi{};
        rpbi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass        = pass.renderPass;
        rpbi.framebuffer       = fb;
        rpbi.renderArea.extent = { mipSize, mipSize };
        rpbi.clearValueCount   = 1;
        rpbi.pClearValues      = &clear;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{ 0, 0, (float)mipSize, (float)mipSize, 0, 1 };
        VkRect2D sc{ {0,0}, {mipSize, mipSize} };
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pass.layout, 0, 1, &pass.set, 0, nullptr);

        // roughness < 0 signals irradiance pass (128-byte PC); >= 0 is prefilter (80-byte PC)
        bool isPrefilter = (roughness >= 0.0f);
        if (isPrefilter) {
            PrefilterPush pc{ kFaceViews[face], roughness };
            vkCmdPushConstants(cmd, pass.layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(pc), &pc);
        } else {
            CubeFacePushBase pc{ kFaceViews[face], kProj };
            vkCmdPushConstants(cmd, pass.layout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(pc), &pc);
        }

        VkBuffer vbs[] = { cubeVB_ };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
        vkCmdBindIndexBuffer(cmd, cubeIB_, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

        vkCmdEndRenderPass(cmd);
        pool.endSingleTimeCommands(cmd);

        vkDestroyFramebuffer(dev, fb, nullptr);
        vkDestroyImageView(dev, faceView, nullptr);
    }
}

// ---------------------------------------------------------------------------
// computeIrradiance
// ---------------------------------------------------------------------------

bool GltfIBLPrecomputer::computeIrradiance(const Phantom::VKG::VulkanContext& ctx,
                                            const Phantom::VKG::VulkanCommandPool& pool,
                                            VkImageView envView,
                                            VkSampler   envSampler,
                                            Result& res)
{
    VkDevice dev = ctx.getDevice();
    constexpr VkFormat fmt = VK_FORMAT_R16G16B16A16_SFLOAT;

    if (!createCubeImage(ctx, kIrradianceSize, 1, fmt, res.irradianceImage, res.irradianceMem))
        return false;

    auto pass = createCubeFacePass(ctx, fmt,
                                   irradianceVertSpv_,
                                   irradianceFragSpv_,
                                   true, sizeof(CubeFacePushBase));
    if (pass.pipeline == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[GltfIBLPrecomputer] irradiance pass creation failed\n");
        return false;
    }

    VkDescriptorImageInfo imgInfo{ envSampler, envView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = pass.set;
    w.dstBinding      = 0;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.descriptorCount = 1;
    w.pImageInfo      = &imgInfo;
    vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);

    renderCubeFaces(ctx, pool, pass, res.irradianceImage, kIrradianceSize, 0, -1.0f);

    destroyCubeFacePass(dev, pass);

    res.irradianceView    = createCubeView(dev, res.irradianceImage, fmt, 1);
    res.irradianceSampler = createLinearSampler(dev, 1);
    return true;
}

// ---------------------------------------------------------------------------
// computePrefilter
// ---------------------------------------------------------------------------

bool GltfIBLPrecomputer::computePrefilter(const Phantom::VKG::VulkanContext& ctx,
                                           const Phantom::VKG::VulkanCommandPool& pool,
                                           VkImageView envView,
                                           VkSampler   envSampler,
                                           Result& res)
{
    VkDevice dev = ctx.getDevice();
    constexpr VkFormat fmt = VK_FORMAT_R16G16B16A16_SFLOAT;

    {
        VkImageCreateInfo ci{};
        ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        ci.imageType     = VK_IMAGE_TYPE_2D;
        ci.format        = fmt;
        ci.extent        = { kPrefilterSize, kPrefilterSize, 1 };
        ci.mipLevels     = kPrefilterMips;
        ci.arrayLayers   = 6;
        ci.samples       = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ci.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        GLTF_IBL_CHECK(vkCreateImage(dev, &ci, nullptr, &res.prefilterImage), "failed to create prefilter image", false);

        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(dev, res.prefilterImage, &mr);
        VkMemoryAllocateInfo ai{};
        auto memType = ctx.findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!memType) {
            std::fprintf(stderr, "[GltfIBLPrecomputer] no suitable memory type\n");
            return false;
        }

        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = *memType;
        GLTF_IBL_CHECK(vkAllocateMemory(dev, &ai, nullptr, &res.prefilterMem), "failed to allocate prefilter image memory", false);
        GLTF_IBL_CHECK(vkBindImageMemory(dev, res.prefilterImage, res.prefilterMem, 0), "failed to bind prefilter image memory", false);
    }

    auto pass = createCubeFacePass(ctx, fmt,
                                   prefilterVertSpv_,
                                   prefilterFragSpv_,
                                   true, sizeof(PrefilterPush));
    if (pass.pipeline == VK_NULL_HANDLE) {
        std::fprintf(stderr, "[GltfIBLPrecomputer] prefilter pass creation failed\n");
        return false;
    }

    VkDescriptorImageInfo imgInfo{ envSampler, envView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = pass.set;
    w.dstBinding      = 0;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.descriptorCount = 1;
    w.pImageInfo      = &imgInfo;
    vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);

    for (uint32_t mip = 0; mip < kPrefilterMips; ++mip) {
        float roughness = static_cast<float>(mip) / static_cast<float>(kPrefilterMips - 1);
        renderCubeFaces(ctx, pool, pass, res.prefilterImage, kPrefilterSize, mip, roughness);
    }

    destroyCubeFacePass(dev, pass);

    res.prefilterView    = createCubeView(dev, res.prefilterImage, fmt, kPrefilterMips);
    res.prefilterSampler = createLinearSampler(dev, kPrefilterMips);
    return true;
}

// ---------------------------------------------------------------------------
// computeBRDFLUT
// ---------------------------------------------------------------------------

bool GltfIBLPrecomputer::computeBRDFLUT(const Phantom::VKG::VulkanContext& ctx,
                                         const Phantom::VKG::VulkanCommandPool& pool,
                                         Result& res)
{
    VkDevice dev = ctx.getDevice();
    constexpr VkFormat fmt = VK_FORMAT_R16G16_SFLOAT;
    constexpr uint32_t sz  = kBRDFLUTSize;

    if (!Phantom::VKG::VulkanImage::create(ctx, sz, sz, fmt,
                              VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                              res.brdfLUTImage, res.brdfLUTMem)) {
        std::fprintf(stderr, "[GltfIBLPrecomputer] failed to create BRDF LUT image\n");
        return false;
    }

    // Render pass
    VkRenderPass rp;
    {
        VkAttachmentDescription att{};
        att.format         = fmt;
        att.samples        = VK_SAMPLE_COUNT_1_BIT;
        att.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        att.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        att.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &ref;

        VkRenderPassCreateInfo rpci{};
        rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpci.attachmentCount = 1;
        rpci.pAttachments    = &att;
        rpci.subpassCount    = 1;
        rpci.pSubpasses      = &subpass;
        GLTF_IBL_CHECK(vkCreateRenderPass(dev, &rpci, nullptr, &rp), "failed to create BRDF LUT render pass", false);
    }

    // Image view + framebuffer
    VkImageView lv;
    {
        VkImageViewCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image    = res.brdfLUTImage;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format   = fmt;
        ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.levelCount = 1;
        ci.subresourceRange.layerCount = 1;
        GLTF_IBL_CHECK(vkCreateImageView(dev, &ci, nullptr, &lv), "failed to create BRDF LUT image view", false);
    }
    VkFramebuffer fb;
    {
        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = rp;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = &lv;
        fbci.width  = sz;
        fbci.height = sz;
        fbci.layers = 1;
        GLTF_IBL_CHECK(vkCreateFramebuffer(dev, &fbci, nullptr, &fb), "failed to create BRDF LUT framebuffer", false);
    }

    // Pipeline (no vertex input, no descriptor set)
    VkPipelineLayout pl;
    VkPipeline pipeline;
    {
        VkPipelineLayoutCreateInfo plci{};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        GLTF_IBL_CHECK(vkCreatePipelineLayout(dev, &plci, nullptr, &pl), "failed to create BRDF LUT pipeline layout", false);

        VkShaderModule vertMod = createShaderModule(dev, brdfVertSpv_);
        VkShaderModule fragMod = createShaderModule(dev, brdfFragSpv_);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                      VK_SHADER_STAGE_VERTEX_BIT, vertMod, "main" };
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                      VK_SHADER_STAGE_FRAGMENT_BIT, fragMod, "main" };

        VkPipelineVertexInputStateCreateInfo vi{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        VkPipelineInputAssemblyStateCreateInfo ia{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };

        VkPipelineViewportStateCreateInfo vs{};
        vs.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1;
        vs.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode    = VK_CULL_MODE_NONE;
        rs.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = 0xF;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments    = &cba;

        VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates    = dynStates;

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
        pci.layout              = pl;
        pci.renderPass          = rp;
        GLTF_IBL_CHECK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline), "failed to create BRDF LUT pipeline", false);

        vkDestroyShaderModule(dev, vertMod, nullptr);
        vkDestroyShaderModule(dev, fragMod, nullptr);
    }

    // Render (fullscreen triangle, no VB)
    {
        VkCommandBuffer cmd = pool.beginSingleTimeCommands();
        VkClearValue clear{};
        clear.color = {0, 0, 0, 1};
        VkRenderPassBeginInfo rpbi{};
        rpbi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpbi.renderPass        = rp;
        rpbi.framebuffer       = fb;
        rpbi.renderArea.extent = { sz, sz };
        rpbi.clearValueCount   = 1;
        rpbi.pClearValues      = &clear;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{ 0, 0, (float)sz, (float)sz, 0, 1 };
        VkRect2D sc{ {0,0}, {sz, sz} };
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
        pool.endSingleTimeCommands(cmd);
    }

    vkDestroyPipeline(dev, pipeline, nullptr);
    vkDestroyPipelineLayout(dev, pl, nullptr);
    vkDestroyFramebuffer(dev, fb, nullptr);
    vkDestroyRenderPass(dev, rp, nullptr);

    res.brdfLUTView    = lv;
    res.brdfLUTSampler = createLinearSampler(dev, 1);
    return true;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

std::optional<GltfIBLPrecomputer::Result> GltfIBLPrecomputer::compute(
    const Phantom::VKG::VulkanContext& ctx,
    const Phantom::VKG::VulkanCommandPool& pool,
    VkImageView envCubeView,
    VkSampler   envSampler,
    Shaders shaders)
{
    if (shaders.irradianceVert.empty() || shaders.irradianceFrag.empty() ||
        shaders.prefilterVert.empty()  || shaders.prefilterFrag.empty()  ||
        shaders.brdfVert.empty()       || shaders.brdfFrag.empty())
        return std::nullopt;

    irradianceVertSpv_ = std::move(shaders.irradianceVert);
    irradianceFragSpv_ = std::move(shaders.irradianceFrag);
    prefilterVertSpv_  = std::move(shaders.prefilterVert);
    prefilterFragSpv_  = std::move(shaders.prefilterFrag);
    brdfVertSpv_       = std::move(shaders.brdfVert);
    brdfFragSpv_       = std::move(shaders.brdfFrag);

    if (!createCubeBuffers(ctx, pool))
        return std::nullopt;

    Result res{};
    bool ok = computeIrradiance(ctx, pool, envCubeView, envSampler, res)
           && computePrefilter (ctx, pool, envCubeView, envSampler, res)
           && computeBRDFLUT   (ctx, pool, res);

    destroyCubeBuffers(ctx.getDevice());

    if (!ok) {
        destroy(ctx.getDevice(), res);
        return std::nullopt;
    }
    return res;
}

void GltfIBLPrecomputer::destroy(VkDevice device, Result& result)
{
    auto destroy1 = [&](VkImage& img, VkDeviceMemory& mem,
                        VkImageView& view, VkSampler& sampler) {
        if (sampler != VK_NULL_HANDLE) { vkDestroySampler(device, sampler, nullptr);   sampler = VK_NULL_HANDLE; }
        if (view    != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr);    view    = VK_NULL_HANDLE; }
        if (img     != VK_NULL_HANDLE) { vkDestroyImage(device, img, nullptr);         img     = VK_NULL_HANDLE; }
        if (mem     != VK_NULL_HANDLE) { vkFreeMemory(device, mem, nullptr);           mem     = VK_NULL_HANDLE; }
    };
    destroy1(result.irradianceImage, result.irradianceMem, result.irradianceView, result.irradianceSampler);
    destroy1(result.prefilterImage,  result.prefilterMem,  result.prefilterView,  result.prefilterSampler);
    destroy1(result.brdfLUTImage,    result.brdfLUTMem,    result.brdfLUTView,    result.brdfLUTSampler);
}
