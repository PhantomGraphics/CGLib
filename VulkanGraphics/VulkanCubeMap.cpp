#include "VulkanCubeMap.h"
#include "VulkanContext.h"
#include "VulkanCommandPool.h"
#include "VulkanImage.h"
#include "detail/VkCheckInternal.h"

// Keep this decoder local: applications can also link GraphicsCore's image reader.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/stb/stb_image.h"

#include <cstring>
#include <vector>

namespace Phantom::VKG {

// ---------------------------------------------------------------------------
// Internal helper: sampler creation (image + view come from VulkanImage::createCubeFromFacesRGBA8)
// ---------------------------------------------------------------------------

bool VulkanCubeMap::createSampler(VkDevice device)
{
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter    = VK_FILTER_LINEAR;
    samplerCI.minFilter    = VK_FILTER_LINEAR;
    samplerCI.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.maxLod       = 0.0f;

    if (vkCreateSampler(device, &samplerCI, nullptr, &sampler_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap: sampler creation failed\n");
        sampler_ = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// create: load 6 faces with stb_image
// ---------------------------------------------------------------------------

bool VulkanCubeMap::create(const VulkanContext& ctx,
                            const VulkanCommandPool& pool,
                            const std::array<std::string, 6>& facePaths)
{
    VkDevice device = ctx.getDevice();

    // Load 6 faces
    int faceW = 0, faceH = 0;
    std::array<stbi_uc*, 6> faceData{};

    for (int i = 0; i < 6; ++i) {
        int w, h, ch;
        faceData[i] = stbi_load(facePaths[i].c_str(), &w, &h, &ch, STBI_rgb_alpha);
        if (!faceData[i]) {
            for (int j = 0; j < i; ++j) stbi_image_free(faceData[j]);
            std::fprintf(stderr, "[VKG] VulkanCubeMap: failed to load %s\n", facePaths[i].c_str());
            return false;
        }
        if (i == 0) {
            faceW = w;
            faceH = h;
            if (faceW <= 0 || faceH <= 0 || faceW != faceH) {
                stbi_image_free(faceData[i]);
                std::fprintf(stderr, "[VKG] VulkanCubeMap: faces must be non-empty square images\n");
                return false;
            }
        } else if (w != faceW || h != faceH) {
            for (int j = 0; j <= i; ++j) stbi_image_free(faceData[j]);
            std::fprintf(stderr,
                         "[VKG] VulkanCubeMap: face dimensions differ (%s is %dx%d, expected %dx%d)\n",
                         facePaths[i].c_str(), w, h, faceW, faceH);
            return false;
        }
    }

    // Pack the six decoded faces back to back, free the decoder buffers, then let the shared
    // helper own staging/image/view (it releases everything and leaves null handles on failure).
    const size_t faceBytes = static_cast<size_t>(faceW) * faceH * 4;
    std::vector<uint8_t> packed(faceBytes * 6);
    for (int i = 0; i < 6; ++i) {
        std::memcpy(packed.data() + faceBytes * i, faceData[i], faceBytes);
        stbi_image_free(faceData[i]);
    }

    if (!VulkanImage::createCubeFromFacesRGBA8(ctx, pool, packed.data(), static_cast<uint32_t>(faceW),
                                               image_, memory_, imageView_)) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap: cube image upload failed\n");
        return false;
    }
    if (!createSampler(device)) {
        destroy(device);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// createDummy: 1x1 black cubemap (no stb_image required)
// ---------------------------------------------------------------------------

bool VulkanCubeMap::createDummy(const VulkanContext& ctx, const VulkanCommandPool& pool)
{
    VkDevice device = ctx.getDevice();

    // 1x1 black RGBA pixel repeated for each of the 6 faces
    const uint8_t kBlack[4] = { 0, 0, 0, 255 };
    uint8_t faces[4 * 6];
    for (int i = 0; i < 6; ++i) std::memcpy(faces + 4 * i, kBlack, 4);

    if (!VulkanImage::createCubeFromFacesRGBA8(ctx, pool, faces, 1, image_, memory_, imageView_)) {
        std::fprintf(stderr, "[VKG] VulkanCubeMap::createDummy: cube image upload failed\n");
        return false;
    }
    if (!createSampler(device)) {
        destroy(device);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// destroy
// ---------------------------------------------------------------------------

void VulkanCubeMap::destroy(VkDevice device)
{
    if (sampler_)   { vkDestroySampler(device, sampler_, nullptr);     sampler_   = VK_NULL_HANDLE; }
    if (imageView_) { vkDestroyImageView(device, imageView_, nullptr); imageView_ = VK_NULL_HANDLE; }
    if (image_)     { vkDestroyImage(device, image_, nullptr);         image_     = VK_NULL_HANDLE; }
    if (memory_)    { vkFreeMemory(device, memory_, nullptr);          memory_    = VK_NULL_HANDLE; }
}

} // namespace VKG
