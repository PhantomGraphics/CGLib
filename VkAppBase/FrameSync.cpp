#include "FrameSync.h"

#include <cstdio>

namespace VKG {

bool FrameSync::create(VkDevice device, uint32_t framesInFlight, uint32_t swapChainImageCount) {
    destroy(device);
    if (!device) return false;

    imageAvailable_.assign(framesInFlight, VK_NULL_HANDLE);
    inFlight_.assign(framesInFlight, VK_NULL_HANDLE);
    renderFinished_.assign(swapChainImageCount, VK_NULL_HANDLE);
    imageFence_.assign(swapChainImageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        if (vkCreateSemaphore(device, &si, nullptr, &imageAvailable_[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fi, nullptr, &inFlight_[i]) != VK_SUCCESS)
        {
            std::fprintf(stderr, "[VKG] Failed to create synchronization objects\n");
            destroy(device);
            return false;
        }
    }
    for (uint32_t i = 0; i < swapChainImageCount; ++i) {
        if (vkCreateSemaphore(device, &si, nullptr, &renderFinished_[i]) != VK_SUCCESS) {
            std::fprintf(stderr, "[VKG] Failed to create render-finished semaphores\n");
            destroy(device);
            return false;
        }
    }
    return true;
}

void FrameSync::destroy(VkDevice device) {
    if (device) {
        for (auto s : imageAvailable_) if (s) vkDestroySemaphore(device, s, nullptr);
        for (auto s : renderFinished_) if (s) vkDestroySemaphore(device, s, nullptr);
        for (auto f : inFlight_)       if (f) vkDestroyFence(device, f, nullptr);
    }
    imageAvailable_.clear();
    renderFinished_.clear();
    inFlight_.clear();
    imageFence_.clear();
}

VkFence FrameSync::claimImage(uint32_t imageIndex, VkFence frameFence) {
    if (imageIndex >= imageFence_.size()) return VK_NULL_HANDLE;
    const VkFence previous = imageFence_[imageIndex];
    imageFence_[imageIndex] = frameFence;
    return previous;
}

void FrameSync::resetImageTable(uint32_t swapChainImageCount) {
    imageFence_.assign(swapChainImageCount, VK_NULL_HANDLE);
}

} // namespace VKG
