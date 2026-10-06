#include "VkAppBase.h"

#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_vulkan.h"

#include "ScreenshotCapture.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string_view>

#ifdef _WIN32
#include <crtdbg.h>
#endif

namespace VKG {

VkAppBase::VkAppBase(int width, int height, const std::string& title,
                     VkSampleCountFlagBits msaaSamples)
    : width_(width), height_(height), title_(title), msaaSamples_(msaaSamples)
{
#ifdef _WIN32
    // Scenario labels/log messages (e.g. ScenarioRunner's fprintf of JSON "label" fields) are
    // plain UTF-8 bytes. Without this, the console falls back to the system ANSI codepage
    // (932/Shift-JIS on Japanese Windows) and any non-ASCII text renders as mojibake.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // assert()/CRT error dialogs block indefinitely on an unattended --run-scenario run
    // (a scenario test process has no one to click the dialog). Route them to stderr
    // instead so a failure surfaces as a visible crash/message rather than a silent hang.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
}

VkAppBase::~VkAppBase() {
    cleanup();
}

bool VkAppBase::run(int argc, char* argv[]) {
    const ScreenshotArgs shot = VKG::parseScreenshotArgs(argc, argv);
    if (!shot.path.empty()) {
        screenshotPath_ = shot.path;
        if (shot.frame) screenshotAtFrame_ = *shot.frame;
        else if (screenshotAtFrame_ < 0) screenshotAtFrame_ = 5;
    }
    return run();
}

bool VkAppBase::run() {
    if (!initVulkan()) {
        cleanup();
        return false;
    }
    onInit();
    onSwapChainCreated();
    initImGui();
    bool ok = mainLoop();
    // Explicit cleanup inside run() while the derived vtable is still valid.
    // ~VkAppBase() also calls cleanup(), but by then the vtable has been
    // rewritten to the base class, so onCleanup() would not dispatch correctly.
    cleanup();
    return ok;
}

// ============================================================
//  IVkSubRenderer defaults
// ============================================================

void VkAppBase::onInit() {
    for (auto* r : subRenderers_)
        r->onInit(context_, commandPool_, renderPass_.get(), MAX_FRAMES_IN_FLIGHT);
}

void VkAppBase::onUpdate(uint32_t frameIndex) {
    for (auto* r : subRenderers_) r->onUpdate(frameIndex);
}

void VkAppBase::onRender(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t /*imageIndex*/) {
    for (auto* r : subRenderers_) r->onRender(cmd, frameIndex);
}

void VkAppBase::onCleanup() {
    VkDevice dev = context_.getDevice();
    for (auto* r : subRenderers_) r->onCleanup(dev);
}

void VkAppBase::onImGui() {
    for (auto* r : subRenderers_) r->onImGui();
    for (auto* p : uiPanels_)    p->onImGui();
}

VkDevice VkAppBase::getDevice() const {
    return context_.getDevice();
}

VkExtent2D VkAppBase::getExtent() const {
    return swapChain_.getExtent();
}

// ============================================================
//  Initialization
// ============================================================

bool VkAppBase::initVulkan() {
    if (!window_.create(width_, height_, title_)) return false;

    // Collect required extensions from GLFW and pass to VulkanContext.
    auto requiredExts = VulkanWindow::getRequiredInstanceExtensions();
    if (!context_.createInstance(title_, requiredExts)) return false;

    surface_ = window_.createSurface(context_.getInstance());
    if (surface_ == VK_NULL_HANDLE) return false;
    if (!context_.initDevice(surface_)) return false;

    if (!commandPool_.init(&context_, surface_)) return false;

    // Pass framebuffer-size callback to VulkanSwapChain (GLFW-independent).
    swapChain_.init(&context_, surface_,
        [this](int& w, int& h) { window_.getFramebufferSize(w, h); });

    // Phase 1: create swap chain and image views.
    if (!swapChain_.createSwapChainAndViews()) return false;

    // Create render pass, querying depth format and MSAA sample count.
    auto depthFormatOpt = swapChain_.findDepthFormat();
    if (!depthFormatOpt.has_value()) {
        std::fprintf(stderr, "[VKG] Failed to find a supported depth format\n");
        return false;
    }
    if (!renderPass_.create(context_, swapChain_.getImageFormat(), *depthFormatOpt, msaaSamples_))
        return false;

    // Phase 2: create MSAA color image + depth resources + framebuffers.
    swapChain_.setMsaaSamples(msaaSamples_);
    if (!swapChain_.createDepthAndFramebuffers(renderPass_.get())) return false;

    commandBuffers_ = commandPool_.allocateCommandBuffers(MAX_FRAMES_IN_FLIGHT);
    return sync_.create(context_.getDevice(), MAX_FRAMES_IN_FLIGHT, swapChain_.getImageCount());
}

// ============================================================
//  ImGui initialization / cleanup
// ============================================================

void VkAppBase::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // GLFW backend: install_callbacks=true lets ImGui handle keyboard/mouse input.
    ImGui_ImplGlfw_InitForVulkan(window_.get(), true);

    // Vulkan backend
    ImGui_ImplVulkan_InitInfo ii{};
    ii.ApiVersion        = VK_API_VERSION_1_3;
    ii.Instance          = context_.getInstance();
    ii.PhysicalDevice    = context_.getPhysicalDevice();
    ii.Device            = context_.getDevice();
    ii.QueueFamily       = context_.getGraphicsQueueFamily();
    ii.Queue             = context_.getGraphicsQueue();
    ii.DescriptorPoolSize = 1000;   // allocate descriptor pool internally
    ii.MinImageCount     = MAX_FRAMES_IN_FLIGHT;
    ii.ImageCount        = swapChain_.getImageCount();
    ii.PipelineInfoMain.RenderPass = renderPass_.get();

    ImGui_ImplVulkan_Init(&ii);

    onImGuiReady();

    // Suppress 3-D view mouse events while ImGui has captured the mouse.
    // Only press events are blocked; release always passes so apps can
    // clean up drag state.  Cursor-pos events are never filtered so that
    // position deltas remain accurate when the cursor re-enters the 3-D view.
    window_.mouseButtonFilter = [](int /*btn*/, int action, int /*mods*/) -> bool {
        if (action == GLFW_PRESS && ImGui::GetIO().WantCaptureMouse) return false;
        return true;
    };
    window_.scrollFilter = [](double /*dx*/, double /*dy*/) -> bool {
        return !ImGui::GetIO().WantCaptureMouse;
    };

    imguiInitialized_ = true;
}

void VkAppBase::cleanupImGui() {
    if (!imguiInitialized_) return;
    vkDeviceWaitIdle(context_.getDevice());
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    imguiInitialized_ = false;
}

// ============================================================
//  Main loop
// ============================================================

bool VkAppBase::mainLoop() {
    bool ok = true;
    while (!window_.shouldClose()) {
        window_.pollEvents();
        // Wait at least 3 frames before checking the exit condition so that
        // the ImGui test engine context is stable (frame 0 can spuriously
        // report IsTestQueueEmpty() == true).
        if (exitCondition_ && frameCount_ >= 3 && exitCondition_())
            window_.close();
        if (!drawFrame()) {
            ok = false;
            break;
        }
        ++frameCount_;
    }
    vkDeviceWaitIdle(context_.getDevice());
    return ok;
}

// ============================================================
//  Frame rendering
// ============================================================

bool VkAppBase::drawFrame() {
    // Opt-in benchmark instrumentation. No extra GPU wait in normal rendering.
    // Read once per process; no class-layout change for existing derived apps.
    static const bool syncRenderTiming = [] {
        const char* value = std::getenv("VKAPP_BENCHMARK_SYNC");
        return value && std::strcmp(value, "1") == 0;
    }();
    static const bool benchmarkNoGui = [] {
        const char* value = std::getenv("VKAPP_BENCHMARK_NO_GUI");
        return value && std::strcmp(value, "1") == 0;
    }();
    VkDevice device = context_.getDevice();
    VkFence  frameFence = sync_.inFlight(currentFrame_);

    vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain_.getSwapChain(),
                                             UINT64_MAX,
                                             sync_.imageAvailable(currentFrame_),
                                             VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return true;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::fprintf(stderr, "[VKG] Failed to acquire swap chain image\n");
        return false;
    }

    if (VkFence prev = sync_.claimImage(imageIndex, frameFence))
        vkWaitForFences(device, 1, &prev, VK_TRUE, UINT64_MAX);

    vkResetFences(device, 1, &frameFence);

    // --- ImGui frame start (before command recording) ---
    if (imguiInitialized_ && !benchmarkNoGui) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    // --- App update & ImGui widget construction ---
    onUpdate(currentFrame_);
    if (imguiInitialized_ && !benchmarkNoGui) {
        onImGui();
        ImGui::Render();
    }

    // --- Command buffer recording ---
    // Exclude acquisition, app/scenario updates, UI construction and present.
    // Include command recording, submit, rendering and the completion fence wait.
    const auto renderStart = syncRenderTiming ? std::chrono::steady_clock::now()
                                             : std::chrono::steady_clock::time_point{};
    VkCommandBuffer cmd = commandBuffers_[currentFrame_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &bi);

    // Record derived-class pre-render (offscreen) commands before the main render pass.
    onPreRender(cmd, currentFrame_);

    VkRenderPassBeginInfo rp{};
    rp.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass  = renderPass_.get();
    rp.framebuffer = swapChain_.getFramebuffers()[imageIndex];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = swapChain_.getExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.05f, 0.05f, 0.05f, 1.f}};
    clearValues[1].depthStencil = {1.f, 0};
    rp.clearValueCount = (uint32_t)clearValues.size();
    rp.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor as dynamic state.
    VkViewport vp{};
    vp.x        = 0.f;
    vp.y        = 0.f;
    vp.width    = (float)swapChain_.getExtent().width;
    vp.height   = (float)swapChain_.getExtent().height;
    vp.minDepth = 0.f;
    vp.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{{0, 0}, swapChain_.getExtent()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Record derived-class render commands.
    onRender(cmd, currentFrame_, imageIndex);

    // Render ImGui draw data inside the render pass (on top of everything).
    if (imguiInitialized_ && !benchmarkNoGui)
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRenderPass(cmd);

    // Screenshot: copy the just-rendered swapchain image to the readback buffer.
    const bool doScreenshot = (!screenshotPath_.empty() &&
                               frameCount_ == screenshotAtFrame_ &&
                               !screenshotDone_);
    if (doScreenshot) {
        const VkExtent2D ext = swapChain_.getExtent();
        const VkDeviceSize bufSize = static_cast<VkDeviceSize>(ext.width) * ext.height * 4;
        if (!screenshotBuffer_.isValid())
            screenshotBuffer_.createMapped(context_, bufSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        recordSwapchainCopyToBuffer(cmd, swapChain_.getImages()[imageIndex], screenshotBuffer_.get(),
                                    {0, 0}, ext);
    }

    // Pixel readback: copy a single pixel from the rendered swapchain image.
    const bool doPixelRead = (pixelReadRequested_ && !pixelReadDone_);
    if (doPixelRead) {
        const VkExtent2D ext = swapChain_.getExtent();
        const uint32_t px = (ext.width  > 0) ? std::min(pixelReadX_, ext.width  - 1) : 0u;
        const uint32_t py = (ext.height > 0) ? std::min(pixelReadY_, ext.height - 1) : 0u;
        if (!pixelReadBuffer_.isValid())
            pixelReadBuffer_.createMapped(context_, 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        recordSwapchainCopyToBuffer(cmd, swapChain_.getImages()[imageIndex], pixelReadBuffer_.get(),
                                    {static_cast<int32_t>(px), static_cast<int32_t>(py)}, {1, 1});
    }

    vkEndCommandBuffer(cmd);

    // Submit
    VkSemaphore          waitSems[]   = { sync_.imageAvailable(currentFrame_) };
    VkSemaphore          signalSems[] = { sync_.renderFinished(imageIndex) };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1;
    si.pWaitSemaphores      = waitSems;
    si.pWaitDstStageMask    = waitStages;
    si.commandBufferCount   = 1;
    si.pCommandBuffers      = &cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores    = signalSems;

    if (vkQueueSubmit(context_.getGraphicsQueue(), 1, &si, frameFence) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to submit draw command buffer\n");
        return false;
    }

    if (syncRenderTiming) {
        if (vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            std::fprintf(stderr, "[VKG] Failed to wait for benchmark render completion\n");
            return false;
        }
        const double elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - renderStart).count();
        // Logging is deliberately outside the timed interval.
        std::fprintf(stdout, "[FrameTiming] frame=%u synchronizedRenderMs=%.6f\n",
                     static_cast<unsigned>(frameCount_), elapsedMs);
    }

    // Present
    VkSwapchainKHR sc = swapChain_.getSwapChain();
    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores    = signalSems;
    pi.swapchainCount     = 1;
    pi.pSwapchains        = &sc;
    pi.pImageIndices      = &imageIndex;

    result = vkQueuePresentKHR(context_.getPresentQueue(), &pi);
    onPostSwap();
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        window_.isResized())
    {
        window_.clearResized();
        recreateSwapChain();
    }

    if (doScreenshot || doPixelRead) {
        vkDeviceWaitIdle(context_.getDevice());
        if (doScreenshot) {
            writeScreenshotFile(swapChain_.getImageFormat(), swapChain_.getExtent());
            screenshotDone_ = true;
        }
        if (doPixelRead) {
            auto* data = static_cast<uint8_t*>(pixelReadBuffer_.getMapped());
            const VkFormat fmt = swapChain_.getImageFormat();
            if (isBgraFormat(fmt)) {
                pixelReadResult_[0] = data[2];
                pixelReadResult_[1] = data[1];
                pixelReadResult_[2] = data[0];
                pixelReadResult_[3] = data[3];
            } else {
                std::memcpy(pixelReadResult_, data, 4);
            }
            pixelReadDone_      = true;
            pixelReadRequested_ = false;
        }
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

void VkAppBase::recreateSwapChain() {
    vkDeviceWaitIdle(context_.getDevice());
    onSwapChainDestroying();
    swapChain_.recreate(renderPass_.get(),
        [this]() { window_.waitEvents(); });
    sync_.create(context_.getDevice(), MAX_FRAMES_IN_FLIGHT, swapChain_.getImageCount());
    if (imguiInitialized_)
        ImGui_ImplVulkan_SetMinImageCount(MAX_FRAMES_IN_FLIGHT);
    onSwapChainCreated();
}

// ============================================================
//  Cleanup
// ============================================================

void VkAppBase::cleanup() {
    if (!context_.getDevice()) return;

    vkDeviceWaitIdle(context_.getDevice());

    onCleanup();     // perform ImGui-dependent teardown (e.g. ImGuiTestEngine_Stop) before cleanupImGui
    cleanupImGui();

    if (screenshotBuffer_.isValid())
        screenshotBuffer_.destroy(context_.getDevice());
    if (pixelReadBuffer_.isValid())
        pixelReadBuffer_.destroy(context_.getDevice());

    sync_.destroy(context_.getDevice());

    commandPool_.freeCommandBuffers(commandBuffers_);
    swapChain_.destroy();
    renderPass_.destroy(context_.getDevice());
    commandPool_.destroy();

    // Destroy surface before device, and device before instance.
    if (surface_) {
        vkDestroySurfaceKHR(context_.getInstance(), surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    context_.destroy();
    window_.destroy();
}

// ============================================================
//  Screenshot helpers
// ============================================================

void VkAppBase::requestScreenshot(const std::string& path) {
    screenshotPath_    = path;
    screenshotAtFrame_ = frameCount_;   // capture this frame's output
    screenshotDone_    = false;
}

void VkAppBase::writeScreenshotFile(VkFormat format, VkExtent2D ext) {
    auto* pixels = static_cast<uint8_t*>(screenshotBuffer_.getMapped());
    if (isBgraFormat(format)) // most surface formats are BGRA on Windows
        swapRedBlue(pixels, static_cast<size_t>(ext.width) * ext.height);

    if (writePng(screenshotPath_, pixels, ext.width, ext.height))
        std::printf("[Screenshot] Saved: %s\n", screenshotPath_.c_str());
    else
        std::fprintf(stderr, "[Screenshot] Failed to write: %s\n", screenshotPath_.c_str());
}

void VkAppBase::requestPixelRead(uint32_t x, uint32_t y) {
    pixelReadX_         = x;
    pixelReadY_         = y;
    pixelReadDone_      = false;
    pixelReadRequested_ = true;
}

bool VkAppBase::pollPixelRead(uint8_t out[4]) {
    if (!pixelReadDone_) return false;
    std::memcpy(out, pixelReadResult_, 4);
    pixelReadDone_ = false;
    return true;
}

} // namespace VKG
