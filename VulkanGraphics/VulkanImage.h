#pragma once

#include <vulkan/vulkan.h>

namespace Phantom::VKG {

class VulkanContext;

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
    /// @return A newly created VkImageView, or VK_NULL_HANDLE on failure.
    static VkImageView createView(VkDevice device, VkImage image,
                                  VkFormat format, VkImageAspectFlags aspect);

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
    /// @return false if image creation or memory allocation fails.
    static bool create(const VulkanContext& ctx,
                       uint32_t width, uint32_t height,
                       VkFormat format, VkImageTiling tiling,
                       VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                       VkImage& image, VkDeviceMemory& memory);
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
