#include "VulkanTestFixture.h"
#include "../VulkanSwapChain.h"

#include <vector>

void VulkanTestFixture::SetUp() {
    ASSERT_TRUE(glfwInit()) << "GLFW init failed";

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window_ = glfwCreateWindow(1, 1, "VulkanGraphicsTest", nullptr, nullptr);
    ASSERT_NE(window_, nullptr) << "Failed to create hidden GLFW window";

    uint32_t extCount = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&extCount);
    std::vector<const char*> extensions(exts, exts + extCount);

    // Validation layers are disabled here: this fixture only checks that
    // handles become valid / invalid, not driver-side validation output.
    ASSERT_TRUE(ctx_.createInstance("VulkanGraphicsTest", extensions, /*enableValidation=*/false))
        << "Failed to create Vulkan instance";

    ASSERT_EQ(glfwCreateWindowSurface(ctx_.getInstance(), window_, nullptr, &surface_), VK_SUCCESS)
        << "Failed to create window surface";

    ASSERT_TRUE(ctx_.initDevice(surface_)) << "Failed to initialize Vulkan device";
    ASSERT_TRUE(pool_.init(&ctx_, surface_)) << "Failed to create command pool";

    Phantom::VKG::VulkanSwapChain sc;
    sc.init(&ctx_, surface_, [](int& w, int& h) { w = 1; h = 1; });
    auto depthFormat = sc.findDepthFormat();
    ASSERT_TRUE(depthFormat.has_value()) << "Failed to find a suitable depth format";
    depthFormat_ = *depthFormat;
}

void VulkanTestFixture::TearDown() {
    pool_.destroy();

    if (surface_ != VK_NULL_HANDLE) {
        // Must be destroyed while ctx_'s VkInstance is still alive.
        vkDestroySurfaceKHR(ctx_.getInstance(), surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    ctx_.destroy();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}
