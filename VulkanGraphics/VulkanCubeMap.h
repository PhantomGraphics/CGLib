#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <string>

namespace Phantom::VKG {

class VulkanContext;
class VulkanCommandPool;

/// @brief Creates a Vulkan cubemap texture from 6 PNG images.
///
/// Face order: right(+X), left(-X), top(+Y), bottom(-Y), front(+Z), back(-Z)
class VulkanCubeMap {
public:
    VulkanCubeMap() = default;
    VulkanCubeMap(const VulkanCubeMap&) = delete;
    VulkanCubeMap& operator=(const VulkanCubeMap&) = delete;
    ~VulkanCubeMap() = default;

    /// @brief Creates a cubemap from 6 PNG files.
    /// @param facePaths Paths to each face in order: {right, left, top, bottom, front, back}.
    /// @return false if any face fails to load or GPU resource creation fails.
    bool create(const VulkanContext& ctx,
                const VulkanCommandPool& pool,
                const std::array<std::string, 6>& facePaths);

    /// @brief Creates a 1x1 black dummy cubemap (no stb_image required).
    /// @return false if GPU resource creation fails.
    bool createDummy(const VulkanContext& ctx, const VulkanCommandPool& pool);

    void destroy(VkDevice device);

    VkImageView getImageView() const { return imageView_; }
    VkSampler   getSampler()   const { return sampler_; }
    bool        isValid()      const { return imageView_ != VK_NULL_HANDLE; }

private:
    VkImage        image_     = VK_NULL_HANDLE;
    VkDeviceMemory memory_    = VK_NULL_HANDLE;
    VkImageView    imageView_ = VK_NULL_HANDLE;
    VkSampler      sampler_   = VK_NULL_HANDLE;

    bool createImage(const VulkanContext& ctx, uint32_t size);
    bool createViewAndSampler(VkDevice device);
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
