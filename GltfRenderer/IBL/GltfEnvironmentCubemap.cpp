#include "GltfEnvironmentCubemap.h"
#include "GltfIBLPrecomputer.h"
#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanImage.h"
#include "../../../CGLib/Graphics/ImageFileReader.h"

#include <glm/gtc/packing.hpp>
#include <cstdio>
#include <cstring>

using namespace Phantom::Gltf;

namespace {
constexpr VkFormat   kFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr uint32_t   kSize   = 1;
constexpr uint32_t   kFaces  = 6;
// Dim sky tint, not black: see GltfEnvironmentCubemap.h's comment for why (a bright flat
// environment washes out every material's own texture detail once IBL is enabled).
constexpr float kSkyColor[4] = { 0.05f, 0.07f, 0.10f, 1.0f };
} // namespace

bool GltfEnvironmentCubemap::create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool)
{
    VkDevice device = ctx.getDevice();
    const VkDeviceSize pixBytes = kSize * kSize * 4 * sizeof(float);

    // The same dim tint repeated for all 6 faces; the helper owns staging and releases everything
    // (leaving null handles) if any step fails.
    std::vector<uint8_t> faces(static_cast<size_t>(pixBytes) * kFaces);
    for (uint32_t f = 0; f < kFaces; ++f)
        std::memcpy(faces.data() + pixBytes * f, kSkyColor, sizeof(kSkyColor));
    if (!Phantom::VKG::VulkanImage::createCubeFromFaces(ctx, pool, faces.data(),
            4 * sizeof(float), kSize, kFormat, image_, memory_, view_))
        return false;

    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod       = 1.0f;
    if (vkCreateSampler(device, &sci, nullptr, &sampler_) != VK_SUCCESS) { destroy(device); return false; }

    return true;
}

void GltfEnvironmentCubemap::destroy(VkDevice device)
{
    if (sampler_) { vkDestroySampler(device, sampler_, nullptr);   sampler_ = VK_NULL_HANDLE; }
    if (view_)    { vkDestroyImageView(device, view_, nullptr);    view_    = VK_NULL_HANDLE; }
    if (image_)   { vkDestroyImage(device, image_, nullptr);       image_   = VK_NULL_HANDLE; }
    if (memory_)  { vkFreeMemory(device, memory_, nullptr);        memory_  = VK_NULL_HANDLE; }
    isRealHDR_ = false;
    hdrPath_.clear();
}

namespace {

// Loads an equirectangular .hdr panorama and uploads it as a 2D VK_FORMAT_R16G16B16A16_SFLOAT
// texture (half float, not the .hdr file's native float32 -- matches the format
// GltfIBLPrecomputer already uses for the cube it bakes from this, and R16G16B16A16_SFLOAT's
// sampled-image-with-linear-filtering support is part of Vulkan's core mandatory format list,
// unlike the 128-bit R32G32B32A32_SFLOAT this would otherwise need to preserve full precision).
// `outImage`/`outMem`/`outView`/`outSampler` may be partially populated even on failure (e.g. the
// image was created but the view failed) -- the caller destroys whichever handles are non-null
// either way. On success the caller owns their lifetime and must destroy them once done with the
// temporary equirect texture (it is only needed for the duration of computeEnvironmentCube()'s
// render pass).
bool createEquirectTexture(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                           const std::string& path,
                           VkImage& outImage, VkDeviceMemory& outMem,
                           VkImageView& outView, VkSampler& outSampler)
{
    Phantom::Graphics::HDRImageFileReader reader;
    if (!reader.read(path)) {
        std::fprintf(stderr, "[GltfEnvironmentCubemap] failed to read HDR file: %s\n", path.c_str());
        return false;
    }
    Phantom::Graphics::Imagef img = reader.toImage();
    const int w = img.getWidth();
    const int h = img.getHeight();
    if (w <= 0 || h <= 0) return false;

    const std::vector<float> px = img.getValues(); // flat RGBA, w*h*4 floats (returned by value)
    std::vector<glm::u16vec4> half(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (size_t i = 0; i < half.size(); ++i) {
        const glm::vec4 c(px[i * 4 + 0], px[i * 4 + 1], px[i * 4 + 2], px[i * 4 + 3]);
        half[i] = glm::packHalf(c);
    }

    constexpr VkFormat fmt = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkDevice device = ctx.getDevice();

    // Shared upload helper: single level, leaves null handles and frees everything on failure.
    if (!Phantom::VKG::VulkanImage::createFromPixels(ctx, pool,
            reinterpret_cast<const uint8_t*>(half.data()), sizeof(glm::u16vec4),
            static_cast<uint32_t>(w), static_cast<uint32_t>(h), fmt, false,
            outImage, outMem, outView))
        return false;

    VkSamplerCreateInfo sci{};
    sci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter    = VK_FILTER_LINEAR;
    sci.minFilter    = VK_FILTER_LINEAR;
    sci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;   // wraps around the horizon seam
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod       = 1.0f;
    if (vkCreateSampler(device, &sci, nullptr, &outSampler) != VK_SUCCESS) return false;

    return true;
}

} // namespace

bool GltfEnvironmentCubemap::loadFromHDR(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                         const std::string& path,
                                         std::vector<uint32_t> equirectVert, std::vector<uint32_t> equirectFrag,
                                         uint32_t cubeSize)
{
    VkDevice device = ctx.getDevice();

    VkImage        eqImage   = VK_NULL_HANDLE;
    VkDeviceMemory eqMem     = VK_NULL_HANDLE;
    VkImageView    eqView    = VK_NULL_HANDLE;
    VkSampler      eqSampler = VK_NULL_HANDLE;
    if (!createEquirectTexture(ctx, pool, path, eqImage, eqMem, eqView, eqSampler)) {
        if (eqSampler) vkDestroySampler(device, eqSampler, nullptr);
        if (eqView)    vkDestroyImageView(device, eqView, nullptr);
        if (eqImage)   vkDestroyImage(device, eqImage, nullptr);
        if (eqMem)     vkFreeMemory(device, eqMem, nullptr);
        return false;
    }

    // Fresh instance per call: GltfIBLPrecomputer holds no state beyond a single compute()/
    // computeEnvironmentCube() invocation's own cube geometry buffers (created/destroyed inside
    // the call), so there is nothing to share across loadFromHDR() calls.
    GltfIBLPrecomputer precomputer;
    auto cube = precomputer.computeEnvironmentCube(ctx, pool, eqView, eqSampler, cubeSize,
                                                    std::move(equirectVert), std::move(equirectFrag));

    vkDestroySampler(device, eqSampler, nullptr);
    vkDestroyImageView(device, eqView, nullptr);
    vkDestroyImage(device, eqImage, nullptr);
    vkFreeMemory(device, eqMem, nullptr);

    if (!cube) {
        std::fprintf(stderr, "[GltfEnvironmentCubemap] equirect-to-cube conversion failed for: %s\n", path.c_str());
        return false;
    }

    destroy(device); // drop whatever cubemap (placeholder or a previous HDRI) was active
    image_   = cube->image;
    memory_  = cube->mem;
    view_    = cube->view;
    sampler_ = cube->sampler;
    isRealHDR_ = true;
    hdrPath_   = path;
    return true;
}

bool GltfEnvironmentCubemap::resetToPlaceholder(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool)
{
    destroy(ctx.getDevice());
    return create(ctx, pool); // create() already leaves isRealHDR_/hdrPath_ at their defaults
}
