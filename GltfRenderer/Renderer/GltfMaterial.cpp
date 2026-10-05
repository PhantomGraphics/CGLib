#include "GltfMaterial.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanImage.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace Phantom::Gltf;

// ============================================================
//  Helpers: create a 2D texture from raw RGBA8 pixels
// ============================================================

// Full mip chain (box-filtered by vkCmdBlitImage) so tiled textures on large surfaces --
// terrain, floors -- minify without shimmering; VulkanImage falls back to a single level if the
// format cannot be linearly blitted. Returns the level count for the sampler's maxLod, or 0 on
// failure (outputs are then left VK_NULL_HANDLE and nothing is leaked).
static uint32_t createTextureFromPixels(
    const Phantom::VKG::VulkanContext& ctx,
    const Phantom::VKG::VulkanCommandPool& pool,
    const uint8_t* pixels, int w, int h,
    VkImage& outImage, VkDeviceMemory& outMemory, VkImageView& outView)
{
    uint32_t mipLevels = 0;
    if (!Phantom::VKG::VulkanImage::createFromPixelsRGBA8(ctx, pool, pixels,
            static_cast<uint32_t>(w), static_cast<uint32_t>(h), true,
            outImage, outMemory, outView, &mipLevels))
        return 0;
    return mipLevels;
}

// ============================================================
//  Fallback 1x1 white texture
// ============================================================

void GltfGpuMaterial::createFallback(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                      VkImage& outImage, VkDeviceMemory& outMemory,
                                      VkImageView& outView)
{
    const uint8_t white[4] = {255, 255, 255, 255};
    createTextureFromPixels(ctx, pool, white, 1, 1, outImage, outMemory, outView);
}

// ============================================================
//  build()
// ============================================================

bool GltfGpuMaterial::uploadTexture(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                     const GltfDocument& doc,
                                     const GltfTextureInfo& texInfo,
                                     uint32_t slotIndex,
                                     VkImageView& outView,
                                     Phantom::VKG::VulkanSampler& outSampler)
{
    if (texInfo.index < 0) return false;
    const auto& tex = doc.textures[texInfo.index];
    if (tex.imageIndex < 0) return false;
    const auto& img = doc.images[tex.imageIndex];
    if (img.pixels.empty()) return false;

    const uint32_t mipLevels = createTextureFromPixels(ctx, pool,
        img.pixels.data(), img.width, img.height,
        texImages_[slotIndex], texMemory_[slotIndex], outView);
    if (mipLevels == 0) return false; // upload failed: nothing was left allocated, slot keeps the fallback
    texViews_[slotIndex] = outView;

    VkFilter filter             = VK_FILTER_LINEAR;
    VkSamplerAddressMode wrap   = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    if (tex.samplerIndex >= 0 && tex.samplerIndex < (int)doc.samplers.size()) {
        const auto& samp = doc.samplers[tex.samplerIndex];
        filter = (samp.magFilter == 9728) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        wrap   = (samp.wrapS == 33071)    ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
               : (samp.wrapS == 33648)    ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                                          : VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
    outSampler.create(ctx.getDevice(), filter, wrap, false, 1.0f, static_cast<float>(mipLevels));
    return true;
}

bool GltfGpuMaterial::build(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                              const GltfDocument& doc,
                              const GltfMaterial& gltfMat,
                              VkDescriptorSetLayout layout,
                              VkDescriptorPool      descPool,
                              VkImageView  fallbackView,
                              VkSampler    fallbackSampler)
{
    fallbackView_    = fallbackView;
    fallbackSampler_ = fallbackSampler;
    doubleSided_     = gltfMat.doubleSided;
    alphaMode_       = gltfMat.alphaMode;

    MaterialUBO uboData{};
    uboData.baseColorFactor         = gltfMat.pbrMetallicRoughness.baseColorFactor;
    uboData.metallicFactor          = gltfMat.pbrMetallicRoughness.metallicFactor;
    uboData.roughnessFactor         = gltfMat.pbrMetallicRoughness.roughnessFactor;
    uboData.normalScale             = gltfMat.normalTexture.scale;
    uboData.occlusionStrength       = gltfMat.occlusionTexture.strength;
    uboData.emissiveFactor          = gltfMat.emissiveFactor;
    uboData.alphaMode               = static_cast<int>(gltfMat.alphaMode);
    uboData.alphaCutoff             = gltfMat.alphaCutoff;
    uboData.hasBaseColorTex         = 0;
    uboData.hasMetallicRoughnessTex = 0;
    uboData.hasNormalTex            = 0;
    uboData.hasOcclusionTex         = 0;
    uboData.hasEmissiveTex          = 0;
    // Only UV0/UV1 are ever uploaded per-vertex (GltfGpuMesh::Vertex); a texCoord>=2 source (no
    // glTF material extension in this codebase's scope actually needs a 3rd UV set) falls back to
    // UV0 rather than sampling garbage.
    auto texCoordSlot = [](int texCoord) { return texCoord == 1 ? 1 : 0; };
    uboData.baseColorTexCoord         = texCoordSlot(gltfMat.pbrMetallicRoughness.baseColorTexture.texCoord);
    uboData.metallicRoughnessTexCoord = texCoordSlot(gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.texCoord);
    uboData.normalTexCoord            = texCoordSlot(gltfMat.normalTexture.texCoord);
    uboData.occlusionTexCoord         = texCoordSlot(gltfMat.occlusionTexture.texCoord);
    uboData.emissiveTexCoord          = texCoordSlot(gltfMat.emissiveTexture.texCoord);

    // KHR_texture_transform per slot -- identity (hasTransform=0) is a no-op in the shader.
    auto writeTransform = [](const GltfTextureInfo& info, int& hasT,
                              float& offX, float& offY, float& scX, float& scY, float& rot) {
        hasT = info.hasTransform ? 1 : 0;
        offX = info.transformOffsetX; offY = info.transformOffsetY;
        scX  = info.transformScaleX;  scY  = info.transformScaleY;
        rot  = info.transformRotation;
    };
    writeTransform(gltfMat.pbrMetallicRoughness.baseColorTexture, uboData.hasBaseColorTransform,
        uboData.baseColorOffsetX, uboData.baseColorOffsetY, uboData.baseColorScaleX, uboData.baseColorScaleY, uboData.baseColorRotation);
    writeTransform(gltfMat.pbrMetallicRoughness.metallicRoughnessTexture, uboData.hasMetallicRoughnessTransform,
        uboData.metallicRoughnessOffsetX, uboData.metallicRoughnessOffsetY, uboData.metallicRoughnessScaleX, uboData.metallicRoughnessScaleY, uboData.metallicRoughnessRotation);
    writeTransform(gltfMat.normalTexture, uboData.hasNormalTransform,
        uboData.normalOffsetX, uboData.normalOffsetY, uboData.normalTransformScaleX, uboData.normalTransformScaleY, uboData.normalRotation);
    writeTransform(gltfMat.occlusionTexture, uboData.hasOcclusionTransform,
        uboData.occlusionOffsetX, uboData.occlusionOffsetY, uboData.occlusionScaleX, uboData.occlusionScaleY, uboData.occlusionRotation);
    writeTransform(gltfMat.emissiveTexture, uboData.hasEmissiveTransform,
        uboData.emissiveOffsetX, uboData.emissiveOffsetY, uboData.emissiveScaleX, uboData.emissiveScaleY, uboData.emissiveRotation);

    // Texture slots: 0=baseColor, 1=metallicRoughness, 2=normal, 3=occlusion, 4=emissive
    VkImageView views[TEXTURE_SLOT_COUNT];
    VkSampler   samps[TEXTURE_SLOT_COUNT];
    for (uint32_t i = 0; i < TEXTURE_SLOT_COUNT; ++i) {
        views[i] = fallbackView;
        samps[i] = fallbackSampler;
    }

    if (uploadTexture(ctx, pool, doc, gltfMat.pbrMetallicRoughness.baseColorTexture,        0, views[0], samplers_[0])) { samps[0] = samplers_[0].get(); uboData.hasBaseColorTex         = 1; }
    if (uploadTexture(ctx, pool, doc, gltfMat.pbrMetallicRoughness.metallicRoughnessTexture, 1, views[1], samplers_[1])) { samps[1] = samplers_[1].get(); uboData.hasMetallicRoughnessTex = 1; }
    if (uploadTexture(ctx, pool, doc, gltfMat.normalTexture,                                 2, views[2], samplers_[2])) { samps[2] = samplers_[2].get(); uboData.hasNormalTex            = 1; }
    if (uploadTexture(ctx, pool, doc, gltfMat.occlusionTexture,                              3, views[3], samplers_[3])) { samps[3] = samplers_[3].get(); uboData.hasOcclusionTex         = 1; }
    if (uploadTexture(ctx, pool, doc, gltfMat.emissiveTexture,                               4, views[4], samplers_[4])) { samps[4] = samplers_[4].get(); uboData.hasEmissiveTex          = 1; }

    for (int f = 0; f < MAX_FRAMES; ++f) {
        ubos_[f].createMapped(ctx, sizeof(MaterialUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        ubos_[f].write(&uboData, sizeof(MaterialUBO));
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES, layout);
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = descPool;
    ai.descriptorSetCount = MAX_FRAMES;
    ai.pSetLayouts        = layouts.data();
    descriptorSets_.resize(MAX_FRAMES);
    if (vkAllocateDescriptorSets(ctx.getDevice(), &ai, descriptorSets_.data()) != VK_SUCCESS) {
        std::fprintf(stderr, "[GltfMaterial] failed to allocate descriptor sets\n");
        return false;
    }

    for (int f = 0; f < MAX_FRAMES; ++f) {
        std::vector<VkWriteDescriptorSet> writes;

        // set=1 binding 0: material UBO
        VkDescriptorBufferInfo matBufInfo{};
        matBufInfo.buffer = ubos_[f].get();
        matBufInfo.offset = 0;
        matBufInfo.range  = sizeof(MaterialUBO);
        VkWriteDescriptorSet matWrite{};
        matWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        matWrite.dstSet          = descriptorSets_[f];
        matWrite.dstBinding      = 0;
        matWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        matWrite.descriptorCount = 1;
        matWrite.pBufferInfo     = &matBufInfo;
        writes.push_back(matWrite);

        // set=1 bindings 1-5: textures
        VkDescriptorImageInfo imgInfos[TEXTURE_SLOT_COUNT];
        for (uint32_t s = 0; s < TEXTURE_SLOT_COUNT; ++s) {
            imgInfos[s].sampler     = samps[s];
            imgInfos[s].imageView   = views[s];
            imgInfos[s].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet w{};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = descriptorSets_[f];
            w.dstBinding      = 1 + s;
            w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo      = &imgInfos[s];
            writes.push_back(w);
        }

        vkUpdateDescriptorSets(ctx.getDevice(),
            static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
    return true;
}

void GltfGpuMaterial::destroy(VkDevice device) {
    for (int f = 0; f < MAX_FRAMES; ++f)
        ubos_[f].destroy(device);

    for (uint32_t s = 0; s < TEXTURE_SLOT_COUNT; ++s) {
        if (samplers_[s].isValid()) samplers_[s].destroy(device);
        if (texViews_[s])  vkDestroyImageView(device, texViews_[s], nullptr);
        if (texImages_[s]) vkDestroyImage(device, texImages_[s], nullptr);
        if (texMemory_[s]) vkFreeMemory(device, texMemory_[s], nullptr);
    }
}
