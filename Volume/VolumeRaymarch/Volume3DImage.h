#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::Volume {

/**
 * @brief Device-local R32_SFLOAT 3D image (sampled + storage + transfer), permanently in
 * VK_IMAGE_LAYOUT_GENERAL so a compute pass can write it and a fragment pass can sample it
 * with only a memory barrier in between. This is the GPU form of ScalarGrid3D.
 */
class Volume3DImage {
public:
    Volume3DImage() = default;
    Volume3DImage(const Volume3DImage&) = delete;
    Volume3DImage& operator=(const Volume3DImage&) = delete;

    /** @brief True if R32_SFLOAT supports linear-filtered sampling and storage on this device. */
    static bool isSupported(const Phantom::VKG::VulkanContext& ctx);

    /** @brief Creates the image (contents undefined until written/uploaded). Returns false on failure. */
    bool create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                uint32_t nx, uint32_t ny, uint32_t nz);
    void destroy(const Phantom::VKG::VulkanContext& ctx);

    /** @brief Blocking upload of nx*ny*nz floats (x fastest, then y, then z). */
    bool upload(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                const float* data, size_t count) const;
    /** @brief Blocking readback (tests / diagnostics; not for per-frame use). */
    bool download(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                  std::vector<float>& out) const;

    /** @brief Barrier: compute-shader writes visible to later compute and fragment reads. */
    void recordWriteToRead(VkCommandBuffer cmd) const;

    VkImage image() const { return image_; }
    VkImageView view() const { return view_; }
    uint32_t nx() const { return nx_; }
    uint32_t ny() const { return ny_; }
    uint32_t nz() const { return nz_; }
    bool isValid() const { return image_ != VK_NULL_HANDLE; }

private:
    VkImage image_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    uint32_t nx_ = 0, ny_ = 0, nz_ = 0;
};

} // namespace Phantom::Volume
