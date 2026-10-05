// Lifecycle contract for the VKG wrapper classes (docs/todo/PLAN_cglib_refactoring.md Phase 3):
//   * destroy() on a never-created object is a no-op and destroy() twice is safe,
//   * create() on a live object releases the previous handles first (no leak),
//   * a failed create() leaves the object invalid.
// A validation-enabled context is used so a leaked handle is reported by the layer when the
// device is destroyed; the captured stderr must then contain no validation message.
#include <gtest/gtest.h>

#include <vulkan/vulkan.h> // must precede glfw3.h

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "../VulkanBuffer.h"
#include "../VulkanCommandPool.h"
#include "../VulkanComputePipeline.h"
#include "../VulkanContext.h"
#include "../VulkanCubeMap.h"
#include "../VulkanDescriptorPool.h"
#include "../VulkanOffscreen.h"
#include "../VulkanPipeline.h"
#include "../VulkanRenderPass.h"
#include "../VulkanSPVLoader.h"
#include "../VulkanSampler.h"
#include "../VulkanSwapChain.h"

#include <string>
#include <vector>

using namespace Phantom::VKG;

namespace {

// Only lifecycle diagnostics count: leaked objects (reported at vkDestroyDevice/Instance) and
// destroy-time VUIDs. The bundled test shaders do not match the layouts used here, so unrelated
// pipeline-interface validation messages are expected and ignored.
bool hasLifecycleError(const std::string& log) {
    return log.find("VUID-vkDestroy") != std::string::npos ||
           log.find("has not been destroyed") != std::string::npos ||
           log.find("Leaked") != std::string::npos;
}

class VulkanLifecycleTest : public ::testing::Test {
protected:
    // Returns false (test skipped) when the validation layer is not installed.
    bool setUpContext() {
        if (!glfwInit()) return false;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window_ = glfwCreateWindow(1, 1, "VulkanLifecycleTest", nullptr, nullptr);
        if (!window_) return false;

        uint32_t extCount = 0;
        const char** exts = glfwGetRequiredInstanceExtensions(&extCount);
        std::vector<const char*> extensions(exts, exts + extCount);

        testing::internal::CaptureStderr();
        const bool ok = ctx_.createInstance("VulkanLifecycleTest", extensions, /*enableValidation=*/true);
        if (!ok) { testing::internal::GetCapturedStderr(); return false; }
        capturing_ = true;

        if (glfwCreateWindowSurface(ctx_.getInstance(), window_, nullptr, &surface_) != VK_SUCCESS) return false;
        if (!ctx_.initDevice(surface_)) return false;
        if (!pool_.init(&ctx_, surface_)) return false;
        return true;
    }

    // Tears everything down and returns the stderr captured since setUpContext().
    std::string finish() {
        pool_.destroy();
        if (surface_ != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(ctx_.getInstance(), surface_, nullptr);
            surface_ = VK_NULL_HANDLE;
        }
        ctx_.destroy();
        std::string log;
        if (capturing_) { log = testing::internal::GetCapturedStderr(); capturing_ = false; }
        if (window_) { glfwDestroyWindow(window_); window_ = nullptr; }
        glfwTerminate();
        return log;
    }

    void TearDown() override { if (window_ || capturing_) finish(); }

    GLFWwindow*    window_    = nullptr;
    VkSurfaceKHR   surface_   = VK_NULL_HANDLE;
    bool           capturing_ = false;
    VulkanContext     ctx_;
    VulkanCommandPool pool_;
};

#define SETUP_OR_SKIP() \
    if (!setUpContext()) GTEST_SKIP() << "validation-enabled Vulkan context unavailable"

} // namespace

TEST_F(VulkanLifecycleTest, DestroyWithoutCreateAndTwiceIsSafe) {
    SETUP_OR_SKIP();
    VkDevice dev = ctx_.getDevice();
    {
        VulkanSampler s;                s.destroy(dev); s.destroy(dev);
        VulkanRenderPass rp;            rp.destroy(dev); rp.destroy(dev);
        VulkanPipeline p;               p.destroy(dev); p.destroy(dev);
        VulkanComputePipeline cp;       cp.destroy(dev); cp.destroy(dev);
        VulkanCubeMap cm;               cm.destroy(dev); cm.destroy(dev);
        VulkanDescriptorSetLayout dl;   dl.destroy(dev); dl.destroy(dev);
        VulkanDescriptorPool dp;        dp.destroy(dev); dp.destroy(dev);
        VulkanBuffer b;                 b.destroy(); b.destroy();
        VulkanOffscreen o;              o.destroy(ctx_); o.destroy(ctx_);
        VulkanCommandPool cmdPool;      cmdPool.destroy(); cmdPool.destroy();
        VulkanSwapChain sc;             sc.destroy();  // init() never called
    }
    const std::string log = finish();
    EXPECT_FALSE(hasLifecycleError(log)) << log;
}

TEST_F(VulkanLifecycleTest, RecreateReleasesPreviousHandles) {
    SETUP_OR_SKIP();
    VkDevice dev = ctx_.getDevice();

    VulkanSampler sampler;
    EXPECT_TRUE(sampler.create(dev));
    EXPECT_TRUE(sampler.create(dev));
    sampler.destroy(dev);

    VulkanDescriptorSetLayout layout;
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    EXPECT_TRUE(layout.create(dev, {binding}));
    EXPECT_TRUE(layout.create(dev, {binding}));

    VulkanDescriptorPool dpool;
    const std::vector<VkDescriptorPoolSize> sizes{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
    EXPECT_TRUE(dpool.create(dev, sizes, 1));
    EXPECT_TRUE(dpool.create(dev, sizes, 1));

    VulkanRenderPass rp;
    EXPECT_TRUE(rp.create(ctx_, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_D32_SFLOAT));
    EXPECT_TRUE(rp.create(ctx_, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_D32_SFLOAT));

    PipelineConfig cfg;
    cfg.vertSpv = loadSPV("shaders/test.vert.spv");
    cfg.fragSpv = loadSPV("shaders/test.frag.spv");
    ASSERT_FALSE(cfg.vertSpv.empty());
    VulkanPipeline pipeline;
    EXPECT_TRUE(pipeline.create(ctx_, rp.get(), cfg));
    EXPECT_TRUE(pipeline.create(ctx_, rp.get(), cfg));

    ComputePipelineConfig ccfg;
    ccfg.compSpv = loadSPV("shaders/test.comp.spv");
    VulkanDescriptorSetLayout computeLayout;  // the test shader reads a storage buffer at set 0
    VkDescriptorSetLayoutBinding sb{};
    sb.binding = 0;
    sb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sb.descriptorCount = 1;
    sb.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    ASSERT_TRUE(computeLayout.create(dev, {sb}));
    ccfg.descriptorSetLayout = computeLayout.get();
    ASSERT_FALSE(ccfg.compSpv.empty());
    VulkanComputePipeline compute;
    EXPECT_TRUE(compute.create(ctx_, ccfg));
    EXPECT_TRUE(compute.create(ctx_, ccfg));

    VulkanCubeMap cube;
    EXPECT_TRUE(cube.createDummy(ctx_, pool_));
    EXPECT_TRUE(cube.createDummy(ctx_, pool_));

    VulkanBuffer buf;
    EXPECT_TRUE(buf.createMapped(ctx_, 64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));
    EXPECT_TRUE(buf.createMapped(ctx_, 64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));

    VulkanOffscreen off;
    EXPECT_TRUE(off.create(ctx_, 32, 32, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT));
    EXPECT_TRUE(off.create(ctx_, 64, 64, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT));
    EXPECT_TRUE(off.isValid());

    // Destroy in reverse dependency order: pipelines before their render pass.
    off.destroy(ctx_);
    buf.destroy();
    cube.destroy(dev);
    compute.destroy(dev);
    computeLayout.destroy(dev);
    pipeline.destroy(dev);
    rp.destroy(dev);
    dpool.destroy(dev);
    layout.destroy(dev);

    const std::string log = finish();
    EXPECT_FALSE(hasLifecycleError(log)) << log;
}

TEST_F(VulkanLifecycleTest, CommandPoolReinitReplacesPool) {
    SETUP_OR_SKIP();
    EXPECT_TRUE(pool_.init(&ctx_, surface_));  // second init must not leak the first pool
    EXPECT_NE(pool_.get(), VK_NULL_HANDLE);
    const std::string log = finish();
    EXPECT_FALSE(hasLifecycleError(log)) << log;
}
