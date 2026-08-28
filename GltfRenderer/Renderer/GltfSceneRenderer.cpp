#include "GltfSceneRenderer.h"
#include "GltfMaterial.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanImage.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace Phantom::Gltf;

// ============================================================
//  Camera helpers
// ============================================================

glm::vec3 GltfSceneRenderer::cameraPosition() const {
    float x = camDist_ * std::sin(camTheta_) * std::cos(camPhi_);
    float y = camDist_ * std::cos(camTheta_);
    float z = camDist_ * std::sin(camTheta_) * std::sin(camPhi_);
    return camTarget_ + glm::vec3(x, y, z);
}

RtCameraParams GltfSceneRenderer::getCameraParams() const {
    return { cameraPosition(), camTarget_, {0.f, 1.f, 0.f}, fovDeg_ };
}

void GltfSceneRenderer::handleMouseButton(bool pressed) {
    isDragging_ = pressed;
}

void GltfSceneRenderer::handleMouseMove(double x, double y) {
    if (isDragging_) {
        float dx = static_cast<float>(x - lastX_) * 0.005f;
        float dy = static_cast<float>(y - lastY_) * 0.005f;
        camPhi_   -= dx;
        camTheta_  = std::max(0.05f, std::min(3.09f, camTheta_ + dy));
    }
    lastX_ = x;
    lastY_ = y;
}

void GltfSceneRenderer::handleScroll(double dy) {
    camDist_ = std::max(0.1f, camDist_ - static_cast<float>(dy) * camDist_ * 0.1f);
}

// ============================================================
//  Fallback cube (1x1 white) for IBL bindings when useIBL=0
// ============================================================

void GltfSceneRenderer::createFallbackCube(const Phantom::VKG::VulkanContext& ctx,
                                            const Phantom::VKG::VulkanCommandPool& pool)
{
    VkDevice dev = ctx.getDevice();

    // Create 1x1 cube image
    VkImageCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ci.imageType     = VK_IMAGE_TYPE_2D;
    ci.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ci.extent        = { 1, 1, 1 };
    ci.mipLevels     = 1;
    ci.arrayLayers   = 6;
    ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ci.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCreateImage(dev, &ci, nullptr, &fallbackCubeImage_);

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(dev, fallbackCubeImage_, &mr);
    auto memType = ctx.findMemoryType(mr.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    assert(memType.has_value());

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = memType.value_or(0);
    vkAllocateMemory(dev, &ai, nullptr, &fallbackCubeMem_);
    vkBindImageMemory(dev, fallbackCubeImage_, fallbackCubeMem_, 0);

    // Upload white pixels to all 6 faces via staging buffer
    const uint8_t white[4] = {255, 255, 255, 255};
    uint8_t pixels[6 * 4];
    for (int i = 0; i < 6; ++i) std::memcpy(pixels + i * 4, white, 4);

    VkBuffer stageBuf; VkDeviceMemory stageMem;
    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = sizeof(pixels);
    bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(dev, &bi, nullptr, &stageBuf);

    vkGetBufferMemoryRequirements(dev, stageBuf, &mr);
    auto stageMemType = ctx.findMemoryType(mr.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    assert(stageMemType.has_value());
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = stageMemType.value_or(0);
    vkAllocateMemory(dev, &ai, nullptr, &stageMem);
    vkBindBufferMemory(dev, stageBuf, stageMem, 0);

    void* mapped;
    vkMapMemory(dev, stageMem, 0, sizeof(pixels), 0, &mapped);
    std::memcpy(mapped, pixels, sizeof(pixels));
    vkUnmapMemory(dev, stageMem);

    VkCommandBuffer cmd = pool.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = fallbackCubeImage_;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
    barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copies[6]{};
    for (int f = 0; f < 6; ++f) {
        copies[f].bufferOffset                    = static_cast<VkDeviceSize>(f * 4);
        copies[f].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copies[f].imageSubresource.layerCount     = 1;
        copies[f].imageSubresource.baseArrayLayer = static_cast<uint32_t>(f);
        copies[f].imageExtent                     = {1, 1, 1};
    }
    vkCmdCopyBufferToImage(cmd, stageBuf, fallbackCubeImage_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, copies);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    pool.endSingleTimeCommands(cmd);

    vkDestroyBuffer(dev, stageBuf, nullptr);
    vkFreeMemory(dev, stageMem, nullptr);

    VkImageViewCreateInfo vci{};
    vci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image    = fallbackCubeImage_;
    vci.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    vci.format   = VK_FORMAT_R8G8B8A8_UNORM;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};
    vkCreateImageView(dev, &vci, nullptr, &fallbackCubeView_);
}

void GltfSceneRenderer::destroyFallbackCube(VkDevice device) {
    if (fallbackCubeView_)  { vkDestroyImageView(device, fallbackCubeView_, nullptr);  fallbackCubeView_  = VK_NULL_HANDLE; }
    if (fallbackCubeImage_) { vkDestroyImage(device, fallbackCubeImage_, nullptr);     fallbackCubeImage_ = VK_NULL_HANDLE; }
    if (fallbackCubeMem_)   { vkFreeMemory(device, fallbackCubeMem_, nullptr);         fallbackCubeMem_   = VK_NULL_HANDLE; }
}

// ============================================================
//  Descriptor layouts
//
//  set=0 (global):
//    binding 0: GlobalUBO         (vert + frag)
//    binding 1: irradianceCube    (frag) — fallback cube when useIBL=0
//    binding 2: prefilteredCube   (frag) — fallback cube when useIBL=0
//    binding 3: brdfLUT sampler2D (frag) — fallback 2D when useIBL=0
//    binding 4: shadowMap sampler2D (frag) — fallback white 2D (always "far") when no shadow map is set
//    binding 5: BoneUBO           (vert) — GPU skinning joint matrices, see updateSkinMatrices()
//
//  set=1 (per-material):
//    binding 0: MaterialUBO       (frag)
//    binding 1-5: 5 textures      (frag)
// ============================================================

void GltfSceneRenderer::createGlobalSetLayout(VkDevice device) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    auto addBinding = [&](uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages) {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = binding;
        b.descriptorType  = type;
        b.descriptorCount = 1;
        b.stageFlags      = stages;
        bindings.push_back(b);
    };

    addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_VERTEX_BIT);

    globalSetLayout_.create(device, bindings);
}

void GltfSceneRenderer::createMaterialSetLayout(VkDevice device) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    auto addBinding = [&](uint32_t binding, VkDescriptorType type, VkShaderStageFlags stages) {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = binding;
        b.descriptorType  = type;
        b.descriptorCount = 1;
        b.stageFlags      = stages;
        bindings.push_back(b);
    };

    addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

    materialSetLayout_.create(device, bindings);
}

bool GltfSceneRenderer::createGlobalDescPool(VkDevice device) {
    // MAX_FRAMES sets: 2 UBOs (GlobalUBO + BoneUBO) + 4 combined_image_samplers each
    // (irradiance/prefiltered/brdfLUT/shadowMap)
    std::vector<VkDescriptorPoolSize> sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         static_cast<uint32_t>(MAX_FRAMES * 2)},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(MAX_FRAMES * 4)},
    };

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = static_cast<uint32_t>(sizes.size());
    ci.pPoolSizes    = sizes.data();
    ci.maxSets       = static_cast<uint32_t>(MAX_FRAMES);

    if (vkCreateDescriptorPool(device, &ci, nullptr, &globalDescPool_) != VK_SUCCESS) {
        fprintf(stderr, "[GltfSceneRenderer] failed to create global descriptor pool\n");
        return false;
    }
    return true;
}

bool GltfSceneRenderer::createGlobalDescriptorSets(VkDevice device) {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES, globalSetLayout_.get());
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = globalDescPool_;
    ai.descriptorSetCount = MAX_FRAMES;
    ai.pSetLayouts        = layouts.data();
    globalDescSets_.resize(MAX_FRAMES);
    if (vkAllocateDescriptorSets(device, &ai, globalDescSets_.data()) != VK_SUCCESS) {
        fprintf(stderr, "[GltfSceneRenderer] failed to allocate global descriptor sets\n");
        return false;
    }
    return true;
}

void GltfSceneRenderer::updateGlobalDescriptorSets(VkDevice device) {
    // Resolve which cube/2D view to bind for IBL slots
    VkImageView cubeView = (envView_ != VK_NULL_HANDLE) ? envView_ : fallbackCubeView_;
    VkSampler   cubeSamp = (envSampler_ != VK_NULL_HANDLE) ? envSampler_ : fallbackSampler_.get();

    for (int f = 0; f < MAX_FRAMES; ++f) {
        std::vector<VkWriteDescriptorSet> writes;

        // binding 0: GlobalUBO
        VkDescriptorBufferInfo uboBufInfo{};
        uboBufInfo.buffer = globalUbos_[f].get();
        uboBufInfo.offset = 0;
        uboBufInfo.range  = sizeof(GlobalUBO);
        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet          = globalDescSets_[f];
        uboWrite.dstBinding      = 0;
        uboWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo     = &uboBufInfo;
        writes.push_back(uboWrite);

        // binding 1: irradianceCube
        VkDescriptorImageInfo irrInfo{ cubeSamp, cubeView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet irrWrite{};
        irrWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        irrWrite.dstSet          = globalDescSets_[f];
        irrWrite.dstBinding      = 1;
        irrWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        irrWrite.descriptorCount = 1;
        irrWrite.pImageInfo      = &irrInfo;
        writes.push_back(irrWrite);

        // binding 2: prefilteredEnvCube
        VkDescriptorImageInfo preInfo{ cubeSamp, cubeView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet preWrite{};
        preWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        preWrite.dstSet          = globalDescSets_[f];
        preWrite.dstBinding      = 2;
        preWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        preWrite.descriptorCount = 1;
        preWrite.pImageInfo      = &preInfo;
        writes.push_back(preWrite);

        // binding 3: brdfLUT (fallback 2D white until IBL is computed)
        VkDescriptorImageInfo lutInfo{ fallbackSampler_.get(), fallbackView_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet lutWrite{};
        lutWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lutWrite.dstSet          = globalDescSets_[f];
        lutWrite.dstBinding      = 3;
        lutWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lutWrite.descriptorCount = 1;
        lutWrite.pImageInfo      = &lutInfo;
        writes.push_back(lutWrite);

        // binding 4: shadowMap (fallback white 2D -- samples as depth=1.0 "far", i.e. never occluded).
        // The real shadow depth view sits in DEPTH_STENCIL_READ_ONLY_OPTIMAL (see
        // VulkanOffscreen's depth attachment finalLayout); the color fallback sits in the
        // usual SHADER_READ_ONLY_OPTIMAL -- the two views need different declared layouts.
        bool hasRealShadow = (shadowView_ != VK_NULL_HANDLE);
        VkImageView   shadowV = hasRealShadow ? shadowView_ : fallbackView_;
        VkSampler     shadowS = hasRealShadow ? shadowSampler_ : fallbackSampler_.get();
        VkImageLayout shadowL = hasRealShadow ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo shadowInfo{ shadowS, shadowV, shadowL };
        VkWriteDescriptorSet shadowWrite{};
        shadowWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        shadowWrite.dstSet          = globalDescSets_[f];
        shadowWrite.dstBinding      = 4;
        shadowWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowWrite.descriptorCount = 1;
        shadowWrite.pImageInfo      = &shadowInfo;
        writes.push_back(shadowWrite);

        // binding 5: BoneUBO
        VkDescriptorBufferInfo boneBufInfo{};
        boneBufInfo.buffer = boneUbos_[f].get();
        boneBufInfo.offset = 0;
        boneBufInfo.range  = sizeof(BoneUBO);
        VkWriteDescriptorSet boneWrite{};
        boneWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        boneWrite.dstSet          = globalDescSets_[f];
        boneWrite.dstBinding      = 5;
        boneWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        boneWrite.descriptorCount = 1;
        boneWrite.pBufferInfo     = &boneBufInfo;
        writes.push_back(boneWrite);

        vkUpdateDescriptorSets(device,
            static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

// ============================================================
//  Material pool (per document)
// ============================================================

bool GltfSceneRenderer::createDescriptorPool(VkDevice device, uint32_t materialCount) {
    uint32_t totalSets = materialCount * static_cast<uint32_t>(MAX_FRAMES);

    std::vector<VkDescriptorPoolSize> sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1 * totalSets},  // MaterialUBO
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5 * totalSets},  // 5 textures
    };

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = static_cast<uint32_t>(sizes.size());
    ci.pPoolSizes    = sizes.data();
    ci.maxSets       = totalSets;

    if (vkCreateDescriptorPool(device, &ci, nullptr, &descriptorPool_) != VK_SUCCESS) {
        fprintf(stderr, "[GltfSceneRenderer] failed to create descriptor pool\n");
        return false;
    }
    return true;
}

// ============================================================
//  Node traversal
// ============================================================

glm::mat4 GltfSceneRenderer::nodeLocalTransform(const GltfNode& node) const {
    if (node.hasMatrix) return node.matrix;
    glm::mat4 T = glm::translate(glm::mat4(1.f), node.translation);
    glm::quat q(node.rotation.w, node.rotation.x, node.rotation.y, node.rotation.z);
    glm::mat4 R = glm::mat4_cast(q);
    glm::mat4 S = glm::scale(glm::mat4(1.f), node.scale);
    return T * R * S;
}

void GltfSceneRenderer::traverseNode(const GltfDocument& doc, int nodeIndex,
                                      const glm::mat4& parentTransform,
                                      const Phantom::VKG::VulkanContext& ctx,
                                      const Phantom::VKG::VulkanCommandPool& pool)
{
    const auto& node  = doc.nodes[nodeIndex];
    glm::mat4   world = parentTransform * nodeLocalTransform(node);

    if (node.meshIndex >= 0 && node.meshIndex < (int)doc.meshes.size()) {
        const auto& mesh = doc.meshes[node.meshIndex];
        // Per glTF spec, a skinned mesh's world position comes entirely from its joint matrices
        // (supplied per-frame via updateSkinMatrices()) -- the mesh-holding node's own transform
        // is ignored, not composed with it. Baking `world` here as well would double-apply it.
        const glm::mat4& bakeTransform = (node.skin >= 0) ? glm::mat4(1.f) : world;
        for (int primIdx = 0; primIdx < static_cast<int>(mesh.primitives.size()); ++primIdx) {
            const auto& prim = mesh.primitives[primIdx];
            if (prim.positionAccessor < 0) continue;
            auto entry = std::make_unique<PrimitiveEntry>();
            entry->materialIndex = prim.materialIndex;
            entry->meshIndex     = node.meshIndex;
            entry->primIndex     = primIdx;
            if (entry->mesh.build(ctx, pool, doc, prim, bakeTransform))
                primitives_.push_back(std::move(entry));
        }
    }

    for (int child : node.children)
        traverseNode(doc, child, world, ctx, pool);
}

// ============================================================
//  IVkSubRenderer::onInit
// ============================================================

void GltfSceneRenderer::onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                VkRenderPass renderPass, uint32_t /*framesInFlight*/)
{
    ctx_  = &ctx;
    pool_ = &pool;
    VkDevice device = ctx.getDevice();

    // Global UBOs (document-independent)
    for (int f = 0; f < MAX_FRAMES; ++f) {
        globalUbos_[f].createMapped(ctx, sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        boneUbos_[f].createMapped(ctx, sizeof(BoneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    // Descriptor layouts
    createGlobalSetLayout(device);
    createMaterialSetLayout(device);

    // Shared fallback resources
    GltfGpuMaterial::createFallback(ctx, pool, fallbackImage_, fallbackMemory_, fallbackView_);
    fallbackSampler_.create(device);
    createFallbackCube(ctx, pool);

    // Global descriptor pool + sets (write fallback IBL textures)
    if (!(createGlobalDescPool(device) && createGlobalDescriptorSets(device)))
        fprintf(stderr, "[GltfSceneRenderer] global descriptor setup failed; rendering may be incomplete\n");
    updateGlobalDescriptorSets(device);

    // Graphics pipeline with 2 descriptor set layouts
    Phantom::VKG::PipelineConfig cfg;
    cfg.vertSpv = std::move(shaders_.vertSpv);
    cfg.fragSpv = std::move(shaders_.fragSpv);
    {
        auto bd = GltfGpuMesh::Vertex::getBindingDescription();
        cfg.bindingDescs = {bd};
        cfg.attrDescs    = GltfGpuMesh::Vertex::getAttributeDescriptions();
    }
    cfg.descriptorSetLayouts = { globalSetLayout_.get(), materialSetLayout_.get() };
    cfg.blendEnable          = false;
    pipeline_.create(ctx, renderPass, cfg);

    // Document-specific resources: only if a document was already set.
    if (doc_) buildDocumentResources();
}

// ============================================================
//  Shadow mapping (Phase C)
// ============================================================

void GltfSceneRenderer::createShadowPipeline(VkRenderPass shadowRenderPass)
{
    if (!ctx_ || shaders_.shadowVertSpv.empty() || shaders_.shadowFragSpv.empty())
        return;

    Phantom::VKG::PipelineConfig cfg;
    cfg.vertSpv = shaders_.shadowVertSpv;
    cfg.fragSpv = shaders_.shadowFragSpv;

    auto bd = GltfGpuMesh::Vertex::getBindingDescription();
    cfg.bindingDescs = { bd };
    // Depth-only: only the position attribute (location 0) is consumed; the interleaved
    // normal/uv/tangent bytes in the same vertex buffer are simply not declared here.
    auto allAttrs = GltfGpuMesh::Vertex::getAttributeDescriptions();
    cfg.attrDescs = { allAttrs[0] };

    cfg.cullMode    = VK_CULL_MODE_NONE; // avoid peter-panning on thin/back-facing casters
    cfg.blendEnable = false;
    cfg.pushConstantRanges = { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) } };

    shadowPipeline_.create(*ctx_, shadowRenderPass, cfg);
}

void GltfSceneRenderer::renderShadowCasters(VkCommandBuffer cmd, const glm::mat4& lightVP)
{
    if (!ready_ || shadowPipeline_.getPipeline() == VK_NULL_HANDLE || primitives_.empty())
        return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_.getPipeline());
    vkCmdPushConstants(cmd, shadowPipeline_.getLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::mat4), &lightVP);

    for (auto& entry : primitives_) {
        VkBuffer     vbuf   = entry->mesh.vertexBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

        if (entry->mesh.hasIndices()) {
            vkCmdBindIndexBuffer(cmd, entry->mesh.indexBuffer(), 0, entry->mesh.indexType());
            vkCmdDrawIndexed(cmd, entry->mesh.indexCount(), 1, 0, 0, 0);
        } else {
            vkCmdDraw(cmd, entry->mesh.vertexCount(), 1, 0, 0);
        }
    }
}

void GltfSceneRenderer::setShadowMap(VkImageView shadowView, VkSampler shadowSampler, const glm::mat4& lightVP)
{
    shadowView_    = shadowView;
    shadowSampler_ = shadowSampler;
    shadowVP_      = lightVP;
    shadowEnabled_ = 1;
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
}

void GltfSceneRenderer::clearShadowMap()
{
    shadowView_    = VK_NULL_HANDLE;
    shadowSampler_ = VK_NULL_HANDLE;
    shadowEnabled_ = 0;
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
}

void GltfSceneRenderer::buildDocumentResources()
{
    VkDevice device = ctx_->getDevice();

    uint32_t matCount = doc_->materials.empty() ? 1u : static_cast<uint32_t>(doc_->materials.size());
    if (!createDescriptorPool(device, matCount))
        fprintf(stderr, "[GltfSceneRenderer] material descriptor pool creation failed; materials may not render\n");

    if (doc_->materials.empty()) {
        GltfMaterial gltfMat;
        auto mat = std::make_unique<GltfGpuMaterial>();
        if (!mat->build(*ctx_, *pool_, *doc_, gltfMat,
                   materialSetLayout_.get(), descriptorPool_,
                   fallbackView_, fallbackSampler_.get()))
            fprintf(stderr, "[GltfSceneRenderer] failed to build default material\n");
        materials_.push_back(std::move(mat));
    } else {
        for (const auto& gltfMat : doc_->materials) {
            auto mat = std::make_unique<GltfGpuMaterial>();
            if (!mat->build(*ctx_, *pool_, *doc_, gltfMat,
                       materialSetLayout_.get(), descriptorPool_,
                       fallbackView_, fallbackSampler_.get()))
                fprintf(stderr, "[GltfSceneRenderer] failed to build material '%s'\n", gltfMat.name.c_str());
            materials_.push_back(std::move(mat));
        }
    }

    int sceneIdx = doc_->defaultScene;
    if (sceneIdx < 0 || sceneIdx >= (int)doc_->scenes.size()) sceneIdx = 0;
    if (!doc_->scenes.empty()) {
        for (int rootNode : doc_->scenes[sceneIdx].nodes)
            traverseNode(*doc_, rootNode, glm::mat4(1.f), *ctx_, *pool_);
    }

    ready_ = true;
}

void GltfSceneRenderer::clearDocumentResources()
{
    if (!ctx_) return;
    VkDevice device = ctx_->getDevice();

    for (auto& entry : primitives_) entry->mesh.destroy(device);
    primitives_.clear();

    for (auto& mat : materials_) mat->destroy(device);
    materials_.clear();

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    ready_ = false;
}

void GltfSceneRenderer::loadDocument(const GltfDocument& doc)
{
    if (ctx_) {
        vkDeviceWaitIdle(ctx_->getDevice());
        clearDocumentResources();
    }
    doc_ = &doc;
    if (ctx_) buildDocumentResources();
}

bool GltfSceneRenderer::updateMorphedPositions(int meshIndex, int primIndex, const std::vector<glm::vec3>& positions)
{
    if (!ready_ || !ctx_ || !pool_) return false;
    for (auto& entry : primitives_) {
        if (entry->meshIndex == meshIndex && entry->primIndex == primIndex)
            return entry->mesh.updatePositions(*ctx_, *pool_, positions);
    }
    return false;
}

void GltfSceneRenderer::setCamera(const glm::mat4& view, const glm::mat4& proj,
                                   const glm::vec3& eye)
{
    extView_ = view;
    extProj_ = proj;
    extEye_  = eye;
    useExternalCamera_ = true;
}

void GltfSceneRenderer::setEnvironment(VkImageView envView, VkSampler envSampler)
{
    envView_    = envView;
    envSampler_ = envSampler;
    // Phase 3: trigger IBL computation and re-write global desc sets here.
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
}

void GltfSceneRenderer::setLight(const glm::vec4& pos, const glm::vec4& color)
{
    lightPos_   = pos;
    lightColor_ = color;
}

// ============================================================
//  IVkSubRenderer::onUpdate  — compute MVP and write GlobalUBO
// ============================================================

void GltfSceneRenderer::onUpdate(uint32_t frameIndex) {
    if (!ready_) return;

    GlobalUBO cam{};
    cam.model          = modelMatrix_;
    cam.lightVP        = shadowVP_;
    cam.lightPos       = lightPos_;
    cam.lightColor     = lightColor_;
    cam.useIBL         = useIBL_;
    cam.shadowEnabled  = shadowEnabled_;
    cam.shadowBias     = shadowBias_;
    cam.shadowStrength = shadowStrength_;

    if (useExternalCamera_) {
        cam.view   = extView_;
        cam.proj   = extProj_;
        cam.camPos = glm::vec4(extEye_, 1.f);
    } else {
        glm::vec3 eye = cameraPosition();
        float aspect = (extent_.height > 0)
            ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
            : 1.f;
        cam.view   = glm::lookAt(eye, camTarget_, glm::vec3(0.f, 1.f, 0.f));
        cam.proj   = glm::perspective(glm::radians(fovDeg_), aspect, 0.001f, 1000.f);
        cam.proj[1][1] *= -1.f; // Vulkan Y flip
        cam.camPos = glm::vec4(eye, 1.f);
    }

    globalUbos_[frameIndex].write(&cam, sizeof(GlobalUBO));

    // BoneUBO: entries beyond skinMatrices_'s size (including the whole array, if
    // updateSkinMatrices() was never called) default to identity -- see BoneUBO's comment.
    BoneUBO bones;
    const size_t suppliedCount = std::min(skinMatrices_.size(), static_cast<size_t>(kMaxGltfBones));
    for (size_t i = 0; i < suppliedCount; ++i)
        bones.bones[i] = skinMatrices_[i];
    for (size_t i = suppliedCount; i < kMaxGltfBones; ++i)
        bones.bones[i] = glm::mat4(1.f);
    boneUbos_[frameIndex].write(&bones, sizeof(BoneUBO));
}

// ============================================================
//  IVkSubRenderer::onRender
// ============================================================

void GltfSceneRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (!ready_ || !visible_ || primitives_.empty()) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    // Bind global descriptor set (set=0) once for all primitives
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1, &globalDescSets_[frameIndex], 0, nullptr);

    for (auto& entry : primitives_) {
        int matIdx = (entry->materialIndex >= 0 && entry->materialIndex < (int)materials_.size())
                   ? entry->materialIndex : 0;
        auto& mat = materials_[matIdx];

        // Bind per-material descriptor set (set=1)
        VkDescriptorSet ds = mat->descriptorSet(frameIndex);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_.getLayout(), 1, 1, &ds, 0, nullptr);

        VkBuffer     vbuf   = entry->mesh.vertexBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

        if (entry->mesh.hasIndices()) {
            vkCmdBindIndexBuffer(cmd, entry->mesh.indexBuffer(), 0, entry->mesh.indexType());
            vkCmdDrawIndexed(cmd, entry->mesh.indexCount(), 1, 0, 0, 0);
        } else {
            vkCmdDraw(cmd, entry->mesh.vertexCount(), 1, 0, 0);
        }
    }
}

// ============================================================
//  IVkSubRenderer::onCleanup
// ============================================================

void GltfSceneRenderer::onCleanup(VkDevice device) {
    if (!ctx_) return;
    ready_ = false;

    pipeline_.destroy(device);
    shadowPipeline_.destroy(device);

    for (auto& entry : primitives_) entry->mesh.destroy(device);
    primitives_.clear();

    for (auto& mat : materials_) mat->destroy(device);
    materials_.clear();

    // Material descriptor pool (document-dependent)
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }

    // Global descriptor pool (document-independent)
    if (globalDescPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, globalDescPool_, nullptr);
        globalDescPool_ = VK_NULL_HANDLE;
    }
    globalDescSets_.clear();

    globalSetLayout_.destroy(device);
    materialSetLayout_.destroy(device);

    // Fallback resources
    destroyFallbackCube(device);
    if (fallbackSampler_.isValid()) fallbackSampler_.destroy(device);
    if (fallbackView_)   vkDestroyImageView(device, fallbackView_, nullptr);
    if (fallbackImage_)  vkDestroyImage(device, fallbackImage_, nullptr);
    if (fallbackMemory_) vkFreeMemory(device, fallbackMemory_, nullptr);
    fallbackView_   = VK_NULL_HANDLE;
    fallbackImage_  = VK_NULL_HANDLE;
    fallbackMemory_ = VK_NULL_HANDLE;

    for (int f = 0; f < MAX_FRAMES; ++f) {
        globalUbos_[f].destroy(device);
        boneUbos_[f].destroy(device);
    }

    ctx_ = nullptr;
}
