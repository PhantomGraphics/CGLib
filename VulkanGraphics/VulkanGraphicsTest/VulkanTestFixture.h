#pragma once

#include <gtest/gtest.h>

#include <vulkan/vulkan.h> // must precede glfw3.h so glfwCreateWindowSurface() is declared

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "../VulkanContext.h"
#include "../VulkanCommandPool.h"

// Headless Vulkan fixture: creates a hidden GLFW window + real VkSurfaceKHR
// so VulkanContext::initDevice() can select a present-capable queue family
// exactly as VkAppBase does, without showing any window on screen.
class VulkanTestFixture : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    GLFWwindow* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    Phantom::VKG::VulkanContext ctx_;
    Phantom::VKG::VulkanCommandPool pool_;

    // Resolved once in SetUp() via VulkanSwapChain::findDepthFormat() so tests
    // that need a depth format (RenderPass/Pipeline/Offscreen) do not each
    // have to stand up their own VulkanSwapChain instance.
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
};
