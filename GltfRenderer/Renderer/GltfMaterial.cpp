#include "GltfMaterial.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanImage.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace Phantom::Gltf;

// ============================================================
//  Helpers: create a 2D texture from raw RGBA8 pixels
// ============================================================

static void createTextureFromPixels(
    const Phantom::VKG::VulkanContext& ctx,
    const Phantom::VKG::VulkanCommandPool& pool,
    const uint8_t* pixels, int w, int h,
    VkImage& outImage, VkDeviceMemory& outMemory, VkImageView& outView)
{
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4;

    // Staging buffer
    VkBuffer stagingBuf;
    VkDeviceMemory stagingMem;
    {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = imageSize;
        bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(ctx.getDevice(), &bi, nullptr, &stagingBuf);

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(ctx.getDevice(), stagingBuf, &mr);
        auto memType = ctx.findMemoryType(mr.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        assert(memType.has_value());

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = mr.size;
        ai.memoryTypeIndex = memType.value_or(0);
        vkAllocateMemory(ctx.getDevice(), &ai, nullptr, &stagingMem);
        vkBindBufferMemory(ctx.getDevice(), stagingBuf, stagingMem, 0);

        void* mapped;
        vkMapMemory(ctx.getDevice(), stagingMem, 0, imageSize, 0, &mapped);
        std::memcpy(mapped, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(ctx.getDevice(), stagingMem);
    }

    Phantom::VKG::VulkanImage::create(ctx,
        static_cast<uint32_t>(w), static_cast<uint32_t>(h),
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        outImage, outMemory);

    auto cmd = pool.beginSingleTimeCommands();
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = outImage;
        barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent      = {(uint32_t)w, (uint32_t)h, 1};
        vkCmdCopyBufferToImage(cmd, stagingBuf, outImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    pool.endSingleTimeCommands(cmd);

    vkDestroyBuffer(ctx.getDevice(), stagingBuf, nullptr);
    vkFreeMemory(ctx.getDevice(), stagingMem, nullptr);

    outView = Phantom::VKG::VulkanImage::createView(ctx.getDevice(), outImage,
                                           VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_ASPECT_COLOR_BIT);
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

    createTextureFromPixels(ctx, pool,
        img.pixels.data(), img.width, img.height,
        texImages_[slotIndex], texMemory_[slotIndex], outView);
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
    outSampler.create(ctx.getDevice(), filter, wrap);
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

    MaterialUBO uboData{};
    uboData.baseColorFactor         = gltfMat.pbrMetallicRoughness.baseColorFactor;
    uboData.metallicFactor          = gltfMat.pbrMetallicRoughness.metallicFactor;
    uboData.roughnessFactor         = gltfMat.pbrMetallicRoughness.roughnessFactor;
    uboData.normalScale             = gltfMat.normalTexture.scale;
    uboData.occlusionStrength       = gltfMat.occlusionTexture.strength;
    uboData.emissiveFactor          = gltfMat.emissiveFactor;
    uboData.hasBaseColorTex         = 0;
    uboData.hasMetallicRoughnessTex = 0;
    uboData.hasNormalTex            = 0;
    uboData.hasOcclusionTex         = 0;
    uboData.hasEmissiveTex          = 0;

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
