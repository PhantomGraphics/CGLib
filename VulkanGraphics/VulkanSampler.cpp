#include "VulkanSampler.h"
#include "detail/VkCheckInternal.h"

namespace Phantom::VKG {

bool VulkanSampler::create(VkDevice device,
                           VkFilter filter,
                           VkSamplerAddressMode addressMode,
                           bool enableAnisotropy,
                           float maxAnisotropy)
{
    VkSamplerCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter = filter;
    ci.minFilter = filter;
    ci.addressModeU = addressMode;
    ci.addressModeV = addressMode;
    ci.addressModeW = addressMode;
    ci.anisotropyEnable = enableAnisotropy ? VK_TRUE : VK_FALSE;
    ci.maxAnisotropy = maxAnisotropy;
    ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    ci.unnormalizedCoordinates = VK_FALSE;
    ci.compareEnable = VK_FALSE;
    ci.compareOp = VK_COMPARE_OP_ALWAYS;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    ci.mipLodBias = 0.0f;
    ci.minLod = 0.0f;
    ci.maxLod = 0.0f;

    VKG_CHECK(vkCreateSampler(device, &ci, nullptr, &sampler_),
              "Failed to create sampler", false);
    return true;
}

void VulkanSampler::destroy(VkDevice device) {
    if (!sampler_) return;
    vkDestroySampler(device, sampler_, nullptr);
    sampler_ = VK_NULL_HANDLE;
}

} // namespace VKG
