#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Phantom::VKG {

class VulkanContext;
class VulkanCommandPool;

/// @brief Static utility functions for creating Vulkan images and image views.
///
/// This struct holds no state; all methods are static factory helpers that reduce
/// the boilerplate associated with VkImage and VkImageView creation.
struct VulkanImage {
    /// @brief Creates a VkImageView for the given image.
    ///
    /// @param device Device that owns the image.
    /// @param image  Image to create a view for.
    /// @param format Format of the image (must match the format used to create the image).
    /// @param aspect Aspect flags (e.g. VK_IMAGE_ASPECT_COLOR_BIT or VK_IMAGE_ASPECT_DEPTH_BIT).
    /// @param mipLevels Number of mip levels the view exposes (from level 0).
    /// @return A newly created VkImageView, or VK_NULL_HANDLE on failure.
    static VkImageView createView(VkDevice device, VkImage image,
                                  VkFormat format, VkImageAspectFlags aspect,
                                  uint32_t mipLevels = 1);

    /// @brief Allocates a 2-D VkImage and binds it to a new VkDeviceMemory allocation.
    ///
    /// @param ctx    Logical device context (used for memory type queries).
    /// @param width  Image width in pixels.
    /// @param height Image height in pixels.
    /// @param format Desired image format.
    /// @param tiling Memory tiling mode (VK_IMAGE_TILING_OPTIMAL recommended for device-only images).
    /// @param usage  Image usage flags (e.g. VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT).
    /// @param props  Required memory property flags (e.g. VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT).
    /// @param image  [out] Receives the created VkImage handle.
    /// @param memory [out] Receives the bound VkDeviceMemory handle.
    /// @param mipLevels Number of mip levels to allocate.
    /// @return false if image creation or memory allocation fails.
    static bool create(const VulkanContext& ctx,
                       uint32_t width, uint32_t height,
                       VkFormat format, VkImageTiling tiling,
                       VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                       VkImage& image, VkDeviceMemory& memory,
                       uint32_t mipLevels = 1);

    /// @brief createFromPixelsRGBA8() for any uncompressed color @p format whose texel is
    ///        @p bytesPerPixel bytes (e.g. VK_FORMAT_R8G8_SNORM with 2). The mip chain, when requested,
    ///        needs the format to support linear blits on this device; otherwise one level is made.
    static bool createFromPixels(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                 const uint8_t* pixels, uint32_t bytesPerPixel,
                                 uint32_t width, uint32_t height, VkFormat format,
                                 bool generateMips,
                                 VkImage& image, VkDeviceMemory& memory, VkImageView& view,
                                 uint32_t* outMipLevels = nullptr);

    /// @brief Uploads six square RGBA8 faces into a new device-local cube image (single mip level,
    ///        VK_IMAGE_VIEW_TYPE_CUBE view, left in SHADER_READ_ONLY_OPTIMAL).
    ///
    /// @param faces Six tightly packed size*size*4-byte faces in +X,-X,+Y,-Y,+Z,-Z order.
    /// Blocks until the upload completes. On failure everything partially created is released and
    /// all outputs are VK_NULL_HANDLE (same contract as createFromPixelsRGBA8). The caller owns the
    /// outputs on success (destroy view, image, then free memory).
    /// @return false on failure.
    static bool createCubeFromFacesRGBA8(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                         const uint8_t* faces, uint32_t size,
                                         VkImage& image, VkDeviceMemory& memory, VkImageView& view);

    /// @brief createCubeFromFacesRGBA8() for any uncompressed color @p format (@p bytesPerPixel
    ///        bytes per texel, e.g. VK_FORMAT_R32G32B32A32_SFLOAT with 16). The format must support
    ///        sampling with the usage this helper requests (SAMPLED | TRANSFER_DST).
    static bool createCubeFromFaces(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                    const uint8_t* faces, uint32_t bytesPerPixel, uint32_t size,
                                    VkFormat format,
                                    VkImage& image, VkDeviceMemory& memory, VkImageView& view);

    /// @brief 1x1, single-layer 2D-array image cleared to zero, left in SHADER_READ_ONLY_OPTIMAL,
    ///        with a VK_IMAGE_VIEW_TYPE_2D_ARRAY view: a "nothing here" stand-in for an optional
    ///        sampler2DArray binding (e.g. an opacity shadow map that does not exist yet).
    /// @return false on failure (all outputs left VK_NULL_HANDLE).
    static bool createZeroArrayTexture(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                       VkFormat format, VkImage& image, VkDeviceMemory& memory,
                                       VkImageView& view);

    /// @brief Uploads tightly packed RGBA8 pixels into a new device-local 2D image
    ///        (VK_FORMAT_R8G8B8A8_UNORM, left in SHADER_READ_ONLY_OPTIMAL) with a full-image view.
    ///
    /// Blocks until the upload completes. With @p generateMips a full mip chain is built by
    /// vkCmdBlitImage (box-filtered); if the format cannot be linearly blitted on this device a
    /// single level is created instead. On failure every partially created object is released and
    /// all outputs are left VK_NULL_HANDLE, so the caller never has to clean up after a false return.
    /// The caller owns the outputs on success (destroy view, image, then free memory).
    /// @param outMipLevels [out, optional] Level count actually created (for a sampler's maxLod).
    /// @return false on failure.
    static bool createFromPixelsRGBA8(const VulkanContext& ctx, const VulkanCommandPool& pool,
                                      const uint8_t* pixels, uint32_t width, uint32_t height,
                                      bool generateMips,
                                      VkImage& image, VkDeviceMemory& memory, VkImageView& view,
                                      uint32_t* outMipLevels = nullptr);
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
