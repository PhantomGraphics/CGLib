#include "VulkanWindow.h"

#include <cstdio>

namespace Phantom::VKG {

VulkanWindow::~VulkanWindow() {
    destroy();
}

bool VulkanWindow::create(int width, int height, const std::string& title) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) {
        std::fprintf(stderr, "[VKG] Failed to create GLFW window\n");
        return false;
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetKeyCallback(window_, keyCallback);
    return true;
}

VkSurfaceKHR VulkanWindow::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window_, nullptr, &surface) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to create window surface\n");
        return VK_NULL_HANDLE;
    }
    return surface;
}

std::vector<const char*> VulkanWindow::getRequiredInstanceExtensions() {
    uint32_t count = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(exts, exts + count);
}

void VulkanWindow::destroy() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

bool VulkanWindow::shouldClose() const { return glfwWindowShouldClose(window_) != 0; }
void VulkanWindow::close()             { glfwSetWindowShouldClose(window_, GLFW_TRUE); }
void VulkanWindow::pollEvents()  const { glfwPollEvents(); }
void VulkanWindow::waitEvents()  const { glfwWaitEvents(); }

void VulkanWindow::getFramebufferSize(int& w, int& h) const {
    glfwGetFramebufferSize(window_, &w, &h);
}

void VulkanWindow::framebufferResizeCallback(GLFWwindow* w, int width, int height) {
    auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(w));
    self->resized_ = true;
    if (self->onResize) self->onResize(width, height);
}

void VulkanWindow::mouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
    auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(w));
    if (self->mouseButtonFilter && !self->mouseButtonFilter(button, action, mods)) return;
    if (self->onMouseButton) self->onMouseButton(button, action, mods);
}

void VulkanWindow::cursorPosCallback(GLFWwindow* w, double x, double y) {
    auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(w));
    if (self->onCursorPos) self->onCursorPos(x, y);
}

void VulkanWindow::scrollCallback(GLFWwindow* w, double xOffset, double yOffset) {
    auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(w));
    if (self->scrollFilter && !self->scrollFilter(xOffset, yOffset)) return;
    if (self->onScroll) self->onScroll(xOffset, yOffset);
}

void VulkanWindow::keyCallback(GLFWwindow* w, int key, int /*scancode*/, int action, int mods) {
    auto* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(w));
    if (self->onKey) self->onKey(key, action, mods);
}

} // namespace Phantom::VKG
