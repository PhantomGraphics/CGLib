#include "GltfSceneRenderer.h"
#include "GltfMaterial.h"
#include "../Gltf/GltfAnimationEvaluator.h"
#include "../Gltf/GltfAccessorView.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanImage.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

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
    addBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_FRAGMENT_BIT); // LightManager::LightBufferGpu

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
    // MAX_FRAMES sets: 3 UBOs (GlobalUBO + BoneUBO + LightBufferGpu) + 4 combined_image_samplers
    // each (irradiance/prefiltered/brdfLUT/shadowMap)
    std::vector<VkDescriptorPoolSize> sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         static_cast<uint32_t>(MAX_FRAMES * 3)},
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
    // Resolve which cube/2D view to bind for IBL slots. Real IBL (iblResult_, see recomputeIBL())
    // wins when available; otherwise fall back to sampling the raw environment cubemap directly
    // for both irradiance and prefiltered (a flat approximation -- no convolution/prefiltering)
    // and a white 2D fallback for the BRDF LUT, exactly as before real IBL existed.
    VkImageView cubeView = (envView_ != VK_NULL_HANDLE) ? envView_ : fallbackCubeView_;
    VkSampler   cubeSamp = (envSampler_ != VK_NULL_HANDLE) ? envSampler_ : fallbackSampler_.get();
    const bool  hasIBL   = iblResult_.isValid();
    VkImageView irrView  = hasIBL ? iblResult_.irradianceView    : cubeView;
    VkSampler   irrSamp  = hasIBL ? iblResult_.irradianceSampler : cubeSamp;
    VkImageView preView  = hasIBL ? iblResult_.prefilterView     : cubeView;
    VkSampler   preSamp  = hasIBL ? iblResult_.prefilterSampler  : cubeSamp;
    VkImageView lutView  = hasIBL ? iblResult_.brdfLUTView       : fallbackView_;
    VkSampler   lutSamp  = hasIBL ? iblResult_.brdfLUTSampler    : fallbackSampler_.get();

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
        VkDescriptorImageInfo irrInfo{ irrSamp, irrView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet irrWrite{};
        irrWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        irrWrite.dstSet          = globalDescSets_[f];
        irrWrite.dstBinding      = 1;
        irrWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        irrWrite.descriptorCount = 1;
        irrWrite.pImageInfo      = &irrInfo;
        writes.push_back(irrWrite);

        // binding 2: prefilteredEnvCube
        VkDescriptorImageInfo preInfo{ preSamp, preView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet preWrite{};
        preWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        preWrite.dstSet          = globalDescSets_[f];
        preWrite.dstBinding      = 2;
        preWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        preWrite.descriptorCount = 1;
        preWrite.pImageInfo      = &preInfo;
        writes.push_back(preWrite);

        // binding 3: brdfLUT (real BRDF LUT if computed, else fallback 2D white)
        VkDescriptorImageInfo lutInfo{ lutSamp, lutView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
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

        // binding 6: LightBufferGpu (multi-light; see setPunctualLights())
        VkDescriptorBufferInfo lightBufInfo{};
        lightBufInfo.buffer = lightUbos_[f].get();
        lightBufInfo.offset = 0;
        lightBufInfo.range  = sizeof(LightManager::LightBufferGpu);
        VkWriteDescriptorSet lightWrite{};
        lightWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lightWrite.dstSet          = globalDescSets_[f];
        lightWrite.dstBinding      = 6;
        lightWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightWrite.descriptorCount = 1;
        lightWrite.pBufferInfo     = &lightBufInfo;
        writes.push_back(lightWrite);

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
        const bool skinned = (node.skin >= 0);
        const glm::mat4& bakeTransform = skinned ? glm::mat4(1.f) : world;

        // Object animation (this document has clips and the mesh isn't skinned): keep a CPU
        // mirror plus the accessor-space positions/normals so onUpdate() can re-bake the whole
        // primitive with its node's animated world matrix each frame.
        const bool objectAnimatable = !skinned && !doc.animations.empty();

        for (int primIdx = 0; primIdx < static_cast<int>(mesh.primitives.size()); ++primIdx) {
            const auto& prim = mesh.primitives[primIdx];
            if (prim.positionAccessor < 0) continue;
            auto entry = std::make_unique<PrimitiveEntry>();
            entry->materialIndex = prim.materialIndex;
            entry->meshIndex     = node.meshIndex;
            entry->primIndex     = primIdx;
            entry->nodeIndex     = nodeIndex;
            entry->restWorld     = bakeTransform;
            entry->mesh.setKeepCpuVertices(dynamic_ || objectAnimatable);

            {
                // AABB center in accessor space -- used only for alpha-BLEND back-to-front sort
                // ordering (onRender()), cheap enough to compute for every primitive unconditionally
                // rather than only when this document turns out to have a BLEND material.
                GltfAccessorView centerView(doc, prim.positionAccessor);
                glm::vec3 mn(std::numeric_limits<float>::max());
                glm::vec3 mx(std::numeric_limits<float>::lowest());
                for (size_t i = 0; i < centerView.count(); ++i) {
                    const glm::vec3 p = centerView.get<glm::vec3>(i);
                    mn = glm::min(mn, p);
                    mx = glm::max(mx, p);
                }
                entry->localCenter = (mn + mx) * 0.5f;
            }

            if (objectAnimatable) {
                GltfAccessorView posView(doc, prim.positionAccessor);
                entry->localPos.resize(posView.count());
                for (size_t i = 0; i < entry->localPos.size(); ++i)
                    entry->localPos[i] = posView.get<glm::vec3>(i);
                if (prim.normalAccessor >= 0) {
                    GltfAccessorView nView(doc, prim.normalAccessor);
                    entry->localNrm.resize(nView.count());
                    for (size_t i = 0; i < entry->localNrm.size(); ++i)
                        entry->localNrm[i] = nView.get<glm::vec3>(i);
                } else {
                    entry->localNrm.assign(entry->localPos.size(), glm::vec3(0.f, 1.f, 0.f));
                }
            }

            if (entry->mesh.build(ctx, pool, doc, prim, bakeTransform))
                primitives_.push_back(std::move(entry));
        }
    }

    for (int child : node.children)
        traverseNode(doc, child, world, ctx, pool);
}

// ============================================================
//  Object animation (node TRS)  — CPU re-bake per frame
// ============================================================

void GltfSceneRenderer::setAnimationClip(int clipIndex)
{
    const int clamped = (doc_ && clipIndex >= 0 && clipIndex < (int)doc_->animations.size()) ? clipIndex : -1;
    if (clamped == animClip_) return;
    animClip_ = clamped;
    animDirty_ = true; // force a re-bake next onUpdate() (to the clip's t=0, or back to rest)

    // Which nodes does this clip *move*? A TRS channel target plus its whole subtree (a spinning
    // pivot carries its children). Everything else stays at its build-time bake. A Weights
    // channel doesn't move its node -- it drives morph targets, applied separately via
    // updateMorphedGeometry() -- so it must not mark the node for a transform re-bake here (that
    // would overwrite the morph with base geometry every frame).
    animatedNode_.assign(doc_ ? doc_->nodes.size() : 0, 0);
    if (animClip_ >= 0) {
        for (const auto& ch : doc_->animations[animClip_].channels) {
            if (ch.target.path == GltfAnimationPath::Weights) continue;
            if (ch.target.node < 0 || ch.target.node >= (int)animatedNode_.size()) continue;
            markSubtreeAnimated(ch.target.node);
        }
    }
}

void GltfSceneRenderer::markSubtreeAnimated(int nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= (int)animatedNode_.size() || animatedNode_[nodeIndex]) return;
    animatedNode_[nodeIndex] = 1;
    for (int child : doc_->nodes[nodeIndex].children)
        markSubtreeAnimated(child);
}

void GltfSceneRenderer::setAnimationTime(float seconds)
{
    if (seconds != animTime_) { animTime_ = seconds; animDirty_ = true; }
}

int GltfSceneRenderer::animationCount() const
{
    return doc_ ? static_cast<int>(doc_->animations.size()) : 0;
}

float GltfSceneRenderer::animationDuration(int clipIndex) const
{
    if (!doc_ || clipIndex < 0 || clipIndex >= (int)doc_->animations.size()) return 0.f;
    return GltfAnimationEvaluator::duration(doc_->animations[clipIndex], *doc_);
}

void GltfSceneRenderer::applyObjectAnimation()
{
    if (!ready_ || !ctx_ || !pool_ || !doc_ || !animDirty_) return;
    animDirty_ = false;

    // Clip disabled: restore every animated primitive to its rest-pose bake once.
    if (animClip_ < 0) {
        for (auto& e : primitives_) {
            if (e->nodeIndex < 0 || e->localPos.empty()) continue;
            e->mesh.setBakeTransform(e->restWorld);
            e->mesh.updatePositionsAndNormals(*ctx_, *pool_, e->localPos, e->localNrm);
        }
        return;
    }

    const std::vector<glm::mat4> globals =
        GltfAnimationEvaluator::evaluateNodeGlobalTransforms(*doc_, animClip_, animTime_);

    for (auto& e : primitives_) {
        if (e->nodeIndex < 0 || e->localPos.empty()) continue;
        if (e->nodeIndex >= (int)globals.size() || e->nodeIndex >= (int)animatedNode_.size()) continue;
        if (!animatedNode_[e->nodeIndex]) continue; // static node -- leave its build-time bake
        e->mesh.setBakeTransform(globals[e->nodeIndex]);
        e->mesh.updatePositionsAndNormals(*ctx_, *pool_, e->localPos, e->localNrm);
    }
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
        lightUbos_[f].createMapped(ctx, sizeof(LightManager::LightBufferGpu), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        lightManager_.uploadUBO(lightUbos_[f]); // seed with the empty buffer (count=0) before the first onUpdate()
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

    // Real IBL, if setEnvironment() was already called (the common pattern -- see its comment):
    // ctx_/pool_ weren't valid yet at that point, so the actual precompute was deferred to here.
    recomputeIBL();
    updateGlobalDescriptorSets(device);

    // Graphics pipeline with 2 descriptor set layouts. Config is reused (not moved-from) below
    // to also build the other 3 variants -- VulkanPipeline::create() takes it by const&.
    Phantom::VKG::PipelineConfig cfg;
    cfg.vertSpv = shaders_.vertSpv;
    cfg.fragSpv = shaders_.fragSpv;
    {
        auto bd = GltfGpuMesh::Vertex::getBindingDescription();
        cfg.bindingDescs = {bd};
        cfg.attrDescs    = GltfGpuMesh::Vertex::getAttributeDescriptions();
    }
    cfg.descriptorSetLayouts = { globalSetLayout_.get(), materialSetLayout_.get() };
    cfg.blendEnable          = false;
    cfg.depthWrite           = true;
    cfg.cullMode             = cullMode_;
    pipeline_.create(ctx, renderPass, cfg);

    cfg.cullMode = VK_CULL_MODE_NONE;
    pipelineDoubleSided_.create(ctx, renderPass, cfg);

    // Alpha BLEND variants: standard src-alpha/one-minus-src-alpha blend, depth test on but depth
    // write off (so overlapping BLEND surfaces don't occlude each other by depth alone -- draw
    // order does that instead, see onRender()'s back-to-front sort).
    cfg.blendEnable = true;
    cfg.depthWrite  = false;
    cfg.cullMode    = cullMode_;
    pipelineBlend_.create(ctx, renderPass, cfg);

    cfg.cullMode = VK_CULL_MODE_NONE;
    pipelineBlendDoubleSided_.create(ctx, renderPass, cfg);

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
    cfg.pushConstantRanges = { VkPushConstantRange{
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) * 2 } };

    shadowPipeline_.create(*ctx_, shadowRenderPass, cfg);
}

void GltfSceneRenderer::renderShadowCasters(VkCommandBuffer cmd, const glm::mat4& lightVP)
{
    if (!ready_ || shadowPipeline_.getPipeline() == VK_NULL_HANDLE || primitives_.empty())
        return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_.getPipeline());
    const std::array<glm::mat4, 2> push{ lightVP, modelMatrix_ };
    vkCmdPushConstants(cmd, shadowPipeline_.getLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(push), push.data());

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

    hasBlendMaterials_ = false;
    for (const auto& mat : materials_) {
        if (mat->isBlend()) { hasBlendMaterials_ = true; break; }
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
    hasBlendMaterials_ = false;
    ready_ = false;
}

void GltfSceneRenderer::loadDocument(const GltfDocument& doc)
{
    if (ctx_) {
        vkDeviceWaitIdle(ctx_->getDevice());
        clearDocumentResources();
    }
    doc_ = &doc;
    // Node/clip indices from the previous document don't carry over.
    animClip_ = -1;
    animTime_ = 0.f;
    animDirty_ = false;
    animatedNode_.clear();
    if (ctx_) buildDocumentResources();
}

bool GltfSceneRenderer::updateMorphedPositions(int meshIndex, int primIndex, const std::vector<glm::vec3>& positions, int nodeIndex)
{
    if (!ready_ || !ctx_ || !pool_) return false;
    applyObjectAnimation(); // Update bake matrices before uploading local-space morph geometry.
    bool updated = false;
    for (auto& entry : primitives_) {
        if (entry->meshIndex != meshIndex || entry->primIndex != primIndex ||
            (nodeIndex >= 0 && entry->nodeIndex != nodeIndex)) continue;
        if (!entry->mesh.updatePositions(*ctx_, *pool_, positions)) return false;
        // Preserve deformation when a later object-animation update re-bakes this entry.
        if (!entry->localPos.empty()) entry->localPos = positions;
        updated = true;
    }
    return updated;
}

bool GltfSceneRenderer::updateMorphedGeometry(int meshIndex, int primIndex,
                                              const std::vector<glm::vec3>& positions,
                                              const std::vector<glm::vec3>& normals, int nodeIndex)
{
    if (!ready_ || !ctx_ || !pool_) return false;
    applyObjectAnimation();
    bool updated = false;
    for (auto& entry : primitives_) {
        if (entry->meshIndex != meshIndex || entry->primIndex != primIndex ||
            (nodeIndex >= 0 && entry->nodeIndex != nodeIndex)) continue;
        if (!entry->mesh.updatePositionsAndNormals(*ctx_, *pool_, positions, normals)) return false;
        if (!entry->localPos.empty()) {
            entry->localPos = positions;
            entry->localNrm = normals;
        }
        updated = true;
    }
    return updated;
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
    recomputeIBL();
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
}

void GltfSceneRenderer::recomputeIBL()
{
    if (!ctx_) return; // deferred: onInit() calls this again once ctx_/pool_ exist

    if (iblResult_.isValid())
        iblPrecomputer_.destroy(ctx_->getDevice(), iblResult_);

    if (envView_ == VK_NULL_HANDLE) return; // no environment set (yet) -- nothing to precompute
    if (auto result = iblPrecomputer_.compute(*ctx_, *pool_, envView_, envSampler_, shaders_.ibl))
        iblResult_ = *result;
    // else: Shaders::ibl was empty or a pass failed -- iblResult_ stays invalid,
    // updateGlobalDescriptorSets() falls back to sampling envView_ directly (old behavior).
}

void GltfSceneRenderer::setLight(const glm::vec4& pos, const glm::vec4& color)
{
    lightPos_   = pos;
    lightColor_ = color;
}

void GltfSceneRenderer::setPunctualLights(std::vector<LightEntry> lights)
{
    lightManager_ = LightManager{}; // clear (LightManager has no bulk-clear method of its own)
    for (const auto& l : lights) {
        if (lightManager_.addLight(l) < 0) {
            std::fprintf(stderr, "[GltfSceneRenderer] setPunctualLights: dropping light(s) beyond "
                                  "LightManager::kMaxLights=%d\n", LightManager::kMaxLights);
            break;
        }
    }
    // Written to lightUbos_ on the next onUpdate(); onInit() already seeded an empty buffer for
    // any frame that renders before that (e.g. the very first frame after a fresh onInit()).
}

// ============================================================
//  IVkSubRenderer::onUpdate  — compute MVP and write GlobalUBO
// ============================================================

void GltfSceneRenderer::onUpdate(uint32_t frameIndex) {
    if (!ready_) return;

    applyObjectAnimation(); // no-op unless a clip is set and the time/clip changed

    GlobalUBO cam{};
    cam.model          = modelMatrix_;
    cam.lightVP        = shadowVP_;
    cam.lightPos       = lightPos_;
    cam.lightColor     = lightColor_;
    cam.useIBL         = useIBL_;
    cam.shadowEnabled  = shadowEnabled_;
    cam.shadowBias     = shadowBias_;
    cam.shadowStrength = shadowStrength_;
    cam.exposure       = exposure_;

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

    lightManager_.uploadUBO(lightUbos_[frameIndex]);
}

// ============================================================
//  IVkSubRenderer::onRender
// ============================================================

void GltfSceneRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (!ready_ || !visible_ || primitives_.empty()) return;

    // Bind global descriptor set (set=0) once for all primitives. All 4 pipeline variants share
    // the same descriptor set layouts (see onInit()), so any of their layouts works here
    // regardless of which one ends up bound first below.
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1, &globalDescSets_[frameIndex], 0, nullptr);

    VkPipeline boundPipeline = VK_NULL_HANDLE; // force the first draw to bind explicitly

    auto materialFor = [&](PrimitiveEntry* entry) -> GltfGpuMaterial* {
        int matIdx = (entry->materialIndex >= 0 && entry->materialIndex < (int)materials_.size())
                   ? entry->materialIndex : 0;
        return materials_[matIdx].get();
    };

    auto draw = [&](PrimitiveEntry* entry, GltfGpuMaterial* mat, Phantom::VKG::VulkanPipeline& matPipeline) {
        if (matPipeline.getPipeline() != boundPipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, matPipeline.getPipeline());
            boundPipeline = matPipeline.getPipeline();
        }

        // Bind per-material descriptor set (set=1)
        VkDescriptorSet ds = mat->descriptorSet(frameIndex);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                matPipeline.getLayout(), 1, 1, &ds, 0, nullptr);

        VkBuffer     vbuf   = entry->mesh.vertexBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

        if (entry->mesh.hasIndices()) {
            vkCmdBindIndexBuffer(cmd, entry->mesh.indexBuffer(), 0, entry->mesh.indexType());
            vkCmdDrawIndexed(cmd, entry->mesh.indexCount(), 1, 0, 0, 0);
        } else {
            vkCmdDraw(cmd, entry->mesh.vertexCount(), 1, 0, 0);
        }
    };

    // Pass 1: opaque + alpha MASK, depth write on, in build (traversal) order. alpha-BLEND
    // primitives are set aside for pass 2 instead of drawn here.
    std::vector<PrimitiveEntry*> blendEntries;
    for (auto& entryPtr : primitives_) {
        PrimitiveEntry* entry = entryPtr.get();
        GltfGpuMaterial* mat = materialFor(entry);
        if (hasBlendMaterials_ && mat->isBlend()) {
            blendEntries.push_back(entry);
            continue;
        }
        draw(entry, mat, mat->doubleSided() ? pipelineDoubleSided_ : pipeline_);
    }

    // Pass 2: alpha BLEND, depth write off, back-to-front (painter's algorithm) so overlapping
    // BLEND surfaces composite correctly against each other, not just against the opaque pass.
    // Sort key is each primitive's rest-pose AABB center transformed by its build-time world
    // matrix -- exact for static geometry, an approximation for an object-animated or skinned
    // BLEND primitive (rare in practice; re-sorting every frame from live transforms would need
    // per-primitive current-world tracking that object animation/skinning don't expose today).
    if (!blendEntries.empty()) {
        const glm::vec3 eye = currentEyePosition();
        std::sort(blendEntries.begin(), blendEntries.end(), [&](PrimitiveEntry* a, PrimitiveEntry* b) {
            const glm::vec3 wa = glm::vec3(modelMatrix_ * a->restWorld * glm::vec4(a->localCenter, 1.f));
            const glm::vec3 wb = glm::vec3(modelMatrix_ * b->restWorld * glm::vec4(b->localCenter, 1.f));
            const glm::vec3 da = wa - eye, db = wb - eye;
            return glm::dot(da, da) > glm::dot(db, db); // farthest first
        });
        for (PrimitiveEntry* entry : blendEntries) {
            GltfGpuMaterial* mat = materialFor(entry);
            draw(entry, mat, mat->doubleSided() ? pipelineBlendDoubleSided_ : pipelineBlend_);
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
    pipelineDoubleSided_.destroy(device);
    pipelineBlend_.destroy(device);
    pipelineBlendDoubleSided_.destroy(device);
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

    // Real IBL (if any was computed)
    if (iblResult_.isValid()) iblPrecomputer_.destroy(device, iblResult_);

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
        lightUbos_[f].destroy(device);
    }

    ctx_ = nullptr;
}
