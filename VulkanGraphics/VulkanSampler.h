#pragma once

#include <vulkan/vulkan.h>

namespace Phantom::VKG {

class VulkanSampler {
public:
    VulkanSampler() = default;
    VulkanSampler(const VulkanSampler&) = delete;
    VulkanSampler& operator=(const VulkanSampler&) = delete;
    ~VulkanSampler() = default;

    /// @return false if sampler creation fails.
    bool create(VkDevice device,
                VkFilter filter = VK_FILTER_LINEAR,
                VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                bool enableAnisotropy = false,
                float maxAnisotropy = 1.0f);

    void destroy(VkDevice device);

    VkSampler get() const { return sampler_; }
    bool isValid() const { return sampler_ != VK_NULL_HANDLE; }

private:
    VkSampler sampler_ = VK_NULL_HANDLE;
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
