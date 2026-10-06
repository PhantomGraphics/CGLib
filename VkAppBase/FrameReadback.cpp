#include "FrameReadback.h"

#include "ScreenshotCapture.h"
#include "../VulkanGraphics/VulkanContext.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace VKG {

void FrameReadback::setScreenshot(const std::string& path, int frame) {
    screenshotPath_    = path;
    screenshotAtFrame_ = frame;
    screenshotDone_    = false;
}

void FrameReadback::requestPixel(uint32_t x, uint32_t y) {
    pixelX_         = x;
    pixelY_         = y;
    pixelDone_      = false;
    pixelRequested_ = true;
}

bool FrameReadback::pollPixel(uint8_t out[4]) {
    if (!pixelDone_) return false;
    std::memcpy(out, pixelResult_, 4);
    pixelDone_ = false;
    return true;
}

FrameReadback::Plan FrameReadback::beginFrame(int frameCount) const {
    Plan p;
    p.screenshot = !screenshotPath_.empty() && frameCount == screenshotAtFrame_ && !screenshotDone_;
    p.pixel      = pixelRequested_ && !pixelDone_;
    return p;
}

VkOffset2D FrameReadback::clampPixel(uint32_t x, uint32_t y, VkExtent2D extent) {
    const uint32_t px = (extent.width  > 0) ? std::min(x, extent.width  - 1) : 0u;
    const uint32_t py = (extent.height > 0) ? std::min(y, extent.height - 1) : 0u;
    return {static_cast<int32_t>(px), static_cast<int32_t>(py)};
}

void FrameReadback::toRgba(const uint8_t* src, VkFormat format, uint8_t out[4]) {
    if (isBgraFormat(format)) {
        out[0] = src[2];
        out[1] = src[1];
        out[2] = src[0];
        out[3] = src[3];
    } else {
        std::memcpy(out, src, 4);
    }
}

FrameReadback::Plan FrameReadback::record(Plan plan, Phantom::VKG::VulkanContext& ctx, VkCommandBuffer cmd,
                           VkImage swapChainImage, VkExtent2D extent, VkFormat format) {
    plan.extent = extent;
    plan.format = format;
    if (!extent.width || !extent.height || !cmd || !swapChainImage) {
        plan.screenshot = plan.pixel = false;
        return plan;
    }
    if (plan.screenshot) {
        const VkDeviceSize bufSize = static_cast<VkDeviceSize>(extent.width) * extent.height * 4;
        if (screenshotBuffer_.isValid() && screenshotBuffer_.getSize() != bufSize)
            screenshotBuffer_.destroy(); // previous captures were completed before finish()
        if (!screenshotBuffer_.isValid())
            plan.screenshot = screenshotBuffer_.createMapped(ctx, bufSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        if (plan.screenshot)
            recordSwapchainCopyToBuffer(cmd, swapChainImage, screenshotBuffer_.get(), {0, 0}, extent);
        else
            std::fprintf(stderr, "[Screenshot] Failed to allocate readback buffer\n");
    }
    if (plan.pixel) {
        const VkOffset2D at = clampPixel(pixelX_, pixelY_, extent);
        if (!pixelBuffer_.isValid())
            plan.pixel = pixelBuffer_.createMapped(ctx, 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        if (plan.pixel)
            recordSwapchainCopyToBuffer(cmd, swapChainImage, pixelBuffer_.get(), at, {1, 1});
        else
            std::fprintf(stderr, "[VKG] Failed to allocate pixel readback buffer\n");
    }
    return plan;
}

void FrameReadback::finish(const Plan& plan) {
    const VkFormat format = plan.format;
    const VkExtent2D extent = plan.extent;
    if (plan.screenshot) {
        auto* pixels = static_cast<uint8_t*>(screenshotBuffer_.getMapped());
        if (isBgraFormat(format)) // most surface formats are BGRA on Windows
            swapRedBlue(pixels, static_cast<size_t>(extent.width) * extent.height);
        if (writePng(screenshotPath_, pixels, extent.width, extent.height))
            std::printf("[Screenshot] Saved: %s\n", screenshotPath_.c_str());
        else
            std::fprintf(stderr, "[Screenshot] Failed to write: %s\n", screenshotPath_.c_str());
        screenshotDone_ = true;
    }
    if (plan.pixel) {
        toRgba(static_cast<const uint8_t*>(pixelBuffer_.getMapped()), format, pixelResult_);
        pixelDone_      = true;
        pixelRequested_ = false;
    }
}

void FrameReadback::destroy(VkDevice device) {
    if (screenshotBuffer_.isValid()) screenshotBuffer_.destroy(device);
    if (pixelBuffer_.isValid())      pixelBuffer_.destroy(device);
}

} // namespace VKG
