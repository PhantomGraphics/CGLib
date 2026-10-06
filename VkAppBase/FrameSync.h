#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace VKG {

// Per-frame synchronization objects of VkAppBase's frame loop: one image-available semaphore and
// one in-flight fence per frame in flight, one render-finished semaphore per swapchain image, and
// the "which frame's fence last used this swapchain image" table. Non-owning of the device;
// create() / destroy() take it. create() on a live object destroys the old objects first, and a
// failed create() releases everything it made.
class FrameSync {
public:
    FrameSync() = default;
    FrameSync(const FrameSync&) = delete;
    FrameSync& operator=(const FrameSync&) = delete;

    // Fences are created signaled so the first frame does not block.
    bool create(VkDevice device, uint32_t framesInFlight, uint32_t swapChainImageCount);
    void destroy(VkDevice device);

    VkSemaphore imageAvailable(uint32_t frame) const { return imageAvailable_[frame]; }
    VkSemaphore renderFinished(uint32_t imageIndex) const { return renderFinished_[imageIndex]; }
    VkFence     inFlight(uint32_t frame) const { return inFlight_[frame]; }

    // Records that `imageIndex` is now used by the frame owning `frameFence` and returns the fence
    // of the previous user (VK_NULL_HANDLE if none or out of range), which the caller must wait on
    // before reusing the image.
    VkFence claimImage(uint32_t imageIndex, VkFence frameFence);

    uint32_t frameCount() const { return static_cast<uint32_t>(inFlight_.size()); }
    uint32_t imageCount() const { return static_cast<uint32_t>(renderFinished_.size()); }

    // Bookkeeping-only setup for tests (no Vulkan objects are created).
    void resetImageTable(uint32_t swapChainImageCount);

private:
    std::vector<VkSemaphore> imageAvailable_;
    std::vector<VkSemaphore> renderFinished_;
    std::vector<VkFence>     inFlight_;
    std::vector<VkFence>     imageFence_;
};

} // namespace VKG
