#pragma once

#include "VkAppBase.h"

#include "../../CGLib/VulkanGraphics/VulkanOffscreen.h"
#include "../../CGLib/VulkanGraphics/VulkanBuffer.h"

#include <array>
#include <cstdint>

namespace VKG {

/// @brief Base class derived from VkAppBase that standardizes a 2-pass offscreen renderer.
///
/// This is the Vulkan counterpart of the OpenGL `RendererBase` (Crystal/AppBase/).
/// It can be used for ID picking, post-processing, shadow maps, etc.
///
/// The class holds a single `VulkanOffscreen` and advances the frame loop like:
///   1. Create an offscreen target of the same size as the swapchain in onSwapChainCreated()
///   2. Record offscreen pass then screen pass in drawFrame()
///   3. In the screen pass you can use `getOffscreen().getColorImageView()` to display results
///
/// Override points for derived classes:
/// @code
///   class MyApp : public VkRendererBase {
///   protected:
///       void onRenderOffscreen(VkCommandBuffer cmd, uint32_t frameIndex) override {
///           // record offscreen pass commands
///       }
///       void onRender(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t imageIndex) override {
///           // composite to screen (e.g. show offscreen result)
///       }
///   };
/// @endcode
class VkRendererBase : public VkAppBase {
public:
    VkRendererBase(int width, int height, const std::string& title)
        : VkAppBase(width, height, title) {}

    ~VkRendererBase() override = default;

protected:
    // -----------------------------------------------------------------
    //  Offscreen pass - override in derived classes
    // -----------------------------------------------------------------

    /// @brief Record draw commands for the offscreen render pass.
    ///
    /// At this point the offscreen render pass has already begun. Calling
    /// `getOffscreen().endRenderPass(cmd)` is not required because it is
    /// handled automatically.
    virtual void onRenderOffscreen(VkCommandBuffer /*cmd*/, uint32_t /*frameIndex*/) {}

    /// @brief Record draw commands for the swapchain (screen) render pass (pure virtual).
    ///
    /// When this is called the swapchain render pass has already begun.
    /// `onRenderOffscreen()` is executed earlier (in `onPreRender()`), and
    /// `getOffscreen().getColorImageView()` is in SHADER_READ_ONLY_OPTIMAL layout.
    virtual void onRenderScreen(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t imageIndex) = 0;

    // -----------------------------------------------------------------
    //  Accessors
    // -----------------------------------------------------------------

    /// @brief Return reference to the offscreen target.
    VulkanOffscreen& getOffscreen() { return offscreen_; }

    /// @brief Color format of the offscreen target (same as screen resolution).
    VkFormat getOffscreenColorFormat() const { return offscreenColorFormat_; }

    // -----------------------------------------------------------------
    //  ID picking - read a pixel's RGBA value from the offscreen buffer
    //  (synchronous, slow)
    // -----------------------------------------------------------------

    /// @brief Read one pixel from the offscreen color buffer to the CPU.
    ///
    /// This issues a `vkQueueWaitIdle` internally, so it is not recommended
    /// to call every frame. It is suitable for occasional calls such as
    /// a mouse click-based pick.
    ///
    /// @param x pixel X coordinate (0 <= x < extent.width)
    /// @param y pixel Y coordinate (0 <= y < extent.height)
    /// @return RGBA 8-bit values. Returns {0,0,0,0} if out of range.
    std::array<uint8_t, 4> readOffscreenPixel(uint32_t x, uint32_t y);

    // -----------------------------------------------------------------
    //  VkAppBase hooks
    // -----------------------------------------------------------------
    void onSwapChainCreated()    override;
    void onSwapChainDestroying() override;
    void onCleanup()             override;

    // Record offscreen pass before the swapchain render pass begins.
    void onPreRender(VkCommandBuffer cmd, uint32_t frameIndex) override final;

    // Delegate to onRenderScreen() inside the swapchain render pass.
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t imageIndex) override final;

private:
    VulkanOffscreen offscreen_;
    VkFormat        offscreenColorFormat_ = VK_FORMAT_R8G8B8A8_UNORM;

    // Host-visible buffer for pixel readback (created lazily if needed).
    VulkanBuffer readbackBuffer_;
    bool         readbackReady_ = false;
};

} // namespace VKG
