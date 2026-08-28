#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// ImGui (Vulkan + GLFW backend)
// Derived classes can override onImGui() to draw widgets.
#include "imgui.h"

#include "VulkanWindow.h"
#include "IVkSubRenderer.h"

#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanSwapChain.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../CGLib/VulkanGraphics/VulkanRenderPass.h"
#include "../../CGLib/VulkanGraphics/VulkanBuffer.h"

#include <functional>
#include <string>
#include <vector>

namespace VKG {

using namespace Phantom::VKG; // VulkanGraphics types migrated to Phantom::VKG

// Base class for Vulkan applications built on GLFW + VulkanGraphics.
//
// Derived classes implement onRender() and may override other hooks as needed.
//
//   class MyApp : public ::VKG::VkAppBase {
//   public:
//       MyApp() : VkAppBase(1280, 720, "My App") {}
//   protected:
//       void onInit()   override { /* create pipelines and buffers */ }
//       void onUpdate(uint32_t frameIndex) override { /* update UBOs */ }
//       void onRender(VkCommandBuffer cmd, uint32_t frameIndex,
//                     uint32_t imageIndex) override { /* record draw commands */ }
//       void onCleanup() override { /* release resources */ }
//   };
class VkAppBase {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    /// @brief Construct the application.
    ///
    /// @param width        Initial window width in pixels.
    /// @param height       Initial window height in pixels.
    /// @param title        Window title string.
    /// @param msaaSamples  MSAA sample count.  Pass VK_SAMPLE_COUNT_4_BIT for 4x MSAA,
    ///                     or VK_SAMPLE_COUNT_1_BIT (default) to disable MSAA.
    ///                     Use VulkanContext::getMaxUsableSampleCount() to query the device maximum.
    VkAppBase(int width, int height, const std::string& title,
              VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT);
    virtual ~VkAppBase();

    // Returns false if Vulkan/window initialization or the render loop hit an
    // unrecoverable error; failures are logged to stderr. Callers may ignore
    // the return value (existing call sites are unaffected).
    bool run();

    // Overload that parses --screenshot <path> and --screenshot-frame <N> from argv.
    bool run(int argc, char* argv[]);

    void setExitCondition(std::function<bool()> fn) { exitCondition_ = std::move(fn); }

    // Request that the next frame be captured as a PNG.
    // Safe to call from onUpdate(). The file is written at the end of that same frame.
    void requestScreenshot(const std::string& path);

    // Request a single-pixel readback of the rendered output.
    // Safe to call from onUpdate(). Result is available the following frame via pollPixelRead().
    void requestPixelRead(uint32_t x, uint32_t y);

    // Returns true (once) when the pixel result from a previous requestPixelRead() is ready.
    // Copies the RGBA bytes into out[4]. Automatically clears the done flag.
    bool pollPixelRead(uint8_t out[4]);

    // Register a sub-renderer whose lifecycle hooks are called by the default
    // onInit / onUpdate / onRender / onCleanup / onImGui implementations.
    // Pointers must remain valid for the lifetime of the application.
    void add(IVkSubRenderer* renderer) { subRenderers_.push_back(renderer); }
    void add(IVkUIPanel*     panel)    { uiPanels_.push_back(panel); }

protected:
    // Hooks for derived classes

    // Called once after Vulkan initialization.
    // Default: calls IVkSubRenderer::onInit() for registered sub-renderers.
    virtual void onInit();

    // Called every frame before rendering.
    // Default: calls IVkSubRenderer::onUpdate() for registered sub-renderers.
    virtual void onUpdate(uint32_t frameIndex);

    // Records draw commands into the command buffer.
    // cmd is passed with render pass begun and viewport/scissor already set.
    // Default: calls IVkSubRenderer::onRender() for registered sub-renderers.
    virtual void onRender(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t imageIndex);

    // Hook called before the swap-chain render pass begins.
    // Record off-screen or compute work here when needed.
    // Default implementation does nothing.
    virtual void onPreRender(VkCommandBuffer /*cmd*/, uint32_t /*frameIndex*/) {}

    // Called right before swap-chain destruction (resize path only).
    // Release resources dependent on extent or render pass here
    // (pipelines, off-screen targets, etc.).
    // Recreate them in onSwapChainCreated().
    virtual void onSwapChainDestroying() {}

    // Called after swap-chain creation/recreation.
    // Runs both on first startup and after resize.
    // Create/update extent-dependent resources here
    // (pipelines, UBO aspect ratio, etc.).
    virtual void onSwapChainCreated() {}

    // Called before Vulkan resource cleanup.
    // Default: calls IVkSubRenderer::onCleanup() for registered sub-renderers.
    virtual void onCleanup();

    // Draw ImGui widgets. Called every frame after ImGui::NewFrame().
    // Default: calls IVkSubRenderer::onImGui() and IVkUIPanel::onImGui().
    virtual void onImGui();

    // Called once right after ImGui initialization completes.
    // Do post-ImGui-context setup here (e.g. ImGuiTestEngine_Start()).
    virtual void onImGuiReady() {}

    // Called every frame immediately after vkQueuePresentKHR.
    // Use for post-present steps like ImGuiTestEngine_PostSwap().
    virtual void onPostSwap() {}

    bool isScreenshotDone() const { return screenshotDone_; }

    // Accessors
    VulkanWindow&      getWindow()      { return window_; }
    VulkanContext&     getContext()      { return context_; }
    VulkanSwapChain&   getSwapChain()   { return swapChain_; }
    VulkanCommandPool& getCommandPool() { return commandPool_; }

    VkDevice              getDevice()       const;
    VkExtent2D            getExtent()       const;
    VkRenderPass          getRenderPass()   const { return renderPass_.get(); }
    uint32_t              getCurrentFrame() const { return currentFrame_; }
    VkSampleCountFlagBits getMsaaSamples()  const { return msaaSamples_; }

private:
    std::vector<IVkSubRenderer*> subRenderers_;
    std::vector<IVkUIPanel*>     uiPanels_;

    int                   width_, height_;
    std::string           title_;
    VkSampleCountFlagBits msaaSamples_ = VK_SAMPLE_COUNT_1_BIT;

    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VulkanWindow      window_;
    VulkanContext     context_;
    VulkanSwapChain   swapChain_;
    VulkanCommandPool commandPool_;
    VulkanRenderPass  renderPass_;

    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore>     imageAvailableSemaphores_;
    std::vector<VkSemaphore>     renderFinishedSemaphores_;
    std::vector<VkFence>         inFlightFences_;
    std::vector<VkFence>         imagesInFlightFences_;
    uint32_t currentFrame_ = 0;
    int      frameCount_   = 0;

    bool imguiInitialized_ = false;
    std::function<bool()> exitCondition_;

    // Screenshot support
    std::string  screenshotPath_;
    int          screenshotAtFrame_ = -1;
    VulkanBuffer screenshotBuffer_;
    bool         screenshotDone_    = false;

    // Pixel readback support
    bool         pixelReadRequested_ = false;
    uint32_t     pixelReadX_         = 0;
    uint32_t     pixelReadY_         = 0;
    bool         pixelReadDone_      = false;
    uint8_t      pixelReadResult_[4] = {};
    VulkanBuffer pixelReadBuffer_;

    bool initVulkan();
    void initImGui();
    void cleanupImGui();
    bool createSyncObjects();
    void destroySyncObjects();
    bool mainLoop();
    bool drawFrame();
    void recreateSwapChain();
    void cleanup();

    void parseScreenshotArgs(int argc, char* argv[]);
    void recordScreenshotCopy(VkCommandBuffer cmd, VkImage srcImage, VkExtent2D ext);
    void writeScreenshotFile(VkFormat format, VkExtent2D ext);
    void recordPixelReadCopy(VkCommandBuffer cmd, VkImage srcImage, uint32_t x, uint32_t y);
};

} // namespace VKG
