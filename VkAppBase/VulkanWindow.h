#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <functional>
#include <string>
#include <vector>

namespace Phantom::VKG {

// Wraps a GLFW window and creates a Vulkan surface.
// Exposes mouse/scroll/resize callbacks via std::function.
class VulkanWindow {
public:
    VulkanWindow() = default;
    VulkanWindow(const VulkanWindow&) = delete;
    VulkanWindow& operator=(const VulkanWindow&) = delete;
    ~VulkanWindow();

    // Returns false (and logs to stderr) if GLFW window creation fails.
    bool create(int width, int height, const std::string& title);

    // Creates a Vulkan surface. Ownership remains with the caller.
    // Returns VK_NULL_HANDLE (and logs to stderr) on failure.
    VkSurfaceKHR createSurface(VkInstance instance) const;

    // Returns Vulkan instance extensions required by GLFW.
    // Pass this to VulkanContext::createInstance() as requiredExtensions.
    static std::vector<const char*> getRequiredInstanceExtensions();

    void destroy();

    GLFWwindow* get()         const { return window_; }
    bool shouldClose()        const;
    void close();
    void pollEvents()         const;
    void waitEvents()         const;
    void getFramebufferSize(int& w, int& h) const;

    bool isResized()    const { return resized_; }
    void clearResized()       { resized_ = false; }

    // Optional callbacks
    std::function<void(int button, int action, int mods)> onMouseButton;
    std::function<void(double x, double y)>               onCursorPos;
    std::function<void(double xOffset, double yOffset)>   onScroll;
    std::function<void(int width, int height)>             onResize;
    std::function<void(int key, int action, int mods)>    onKey;

    // Optional pre-filters. Return false to block the event before the callback fires.
    // VkAppBase sets these after ImGui initialisation to suppress 3-D view events
    // when ImGui has captured the mouse.  Left unset, all events pass through.
    std::function<bool(int button, int action, int mods)> mouseButtonFilter;
    std::function<bool(double xOffset, double yOffset)>   scrollFilter;

private:
    GLFWwindow* window_  = nullptr;
    bool        resized_ = false;

    static void framebufferResizeCallback(GLFWwindow* w, int width, int height);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double x, double y);
    static void scrollCallback(GLFWwindow* w, double xOffset, double yOffset);
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
};

} // namespace VKG
