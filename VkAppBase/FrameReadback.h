#pragma once

#include <vulkan/vulkan.h>

#include "../VulkanGraphics/VulkanBuffer.h"

#include <cstdint>
#include <string>

namespace Phantom::VKG { class VulkanContext; }

namespace VKG {

// Request state and GPU readback buffers for VkAppBase's screenshot and single-pixel readback.
// Per frame, VkAppBase asks beginFrame() what to capture, records the copies into the frame's
// command buffer with record(), and after the GPU finished (device idle) calls finish().
class FrameReadback {
public:
    // Screenshot of the frame numbered `frame` (frames are VkAppBase::frameCount_).
    void setScreenshot(const std::string& path, int frame);
    // First call wins its frame default: sets `frame` only when none is configured yet.
    void setScreenshotDefaultFrame(int frame) { if (screenshotAtFrame_ < 0) screenshotAtFrame_ = frame; }
    const std::string& screenshotPath() const { return screenshotPath_; }
    int  screenshotFrame() const { return screenshotAtFrame_; }
    bool screenshotDone() const { return screenshotDone_; }

    void requestPixel(uint32_t x, uint32_t y);
    // True once per completed readback; copies RGBA into out[4].
    bool pollPixel(uint8_t out[4]);

    struct Plan {
        bool screenshot = false;
        bool pixel = false;
        VkExtent2D extent{};
        VkFormat format = VK_FORMAT_UNDEFINED;
    };
    Plan beginFrame(int frameCount) const;

    // Records the copies selected by `plan` after the render pass ended (buffers are created lazily).
    Plan record(Plan plan, Phantom::VKG::VulkanContext& ctx, VkCommandBuffer cmd,
                VkImage swapChainImage, VkExtent2D extent, VkFormat format);

    // Reads the buffers back. Only call after the GPU finished the recorded command buffer.
    void finish(const Plan& plan);

    void destroy(VkDevice device);

    // 4-byte pixel as stored in a swapchain image of `format` -> RGBA.
    static void toRgba(const uint8_t* src, VkFormat format, uint8_t out[4]);
    // Clamps (x, y) into the extent (0 for an empty extent).
    static VkOffset2D clampPixel(uint32_t x, uint32_t y, VkExtent2D extent);

private:
    std::string  screenshotPath_;
    int          screenshotAtFrame_ = -1;
    bool         screenshotDone_    = false;
    Phantom::VKG::VulkanBuffer screenshotBuffer_;

    bool         pixelRequested_ = false;
    bool         pixelDone_      = false;
    uint32_t     pixelX_ = 0, pixelY_ = 0;
    uint8_t      pixelResult_[4] = {};
    Phantom::VKG::VulkanBuffer pixelBuffer_;
};

} // namespace VKG
