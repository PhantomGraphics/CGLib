#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::Animation {

struct GpuTexture {
    VkImage        image   = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;
    VkImageView    view    = VK_NULL_HANDLE;
    VkSampler      sampler = VK_NULL_HANDLE;
};

class VulkanTextureHelper {
public:
    bool createFallback(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool);

    const GpuTexture& load(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                           const std::string& absPath);

    const GpuTexture& fallback() const { return fallback_; }

    void destroyAll(VkDevice device);

private:
    std::unordered_map<std::string, GpuTexture> cache_;
    GpuTexture fallback_;

    static GpuTexture uploadPixels(const Phantom::VKG::VulkanContext& ctx,
                                    const Phantom::VKG::VulkanCommandPool& pool,
                                    const uint8_t* pixels, int w, int h);
};

} // namespace Phantom::Animation
