#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace VKG {

using namespace Phantom::VKG;

// Lifecycle-aware sub-renderer registered with VkAppBase::add().
// All methods have empty default implementations; override only what is needed.
//
// Lifecycle order per VkAppBase::run():
//   onInit()            - once after Vulkan init
//   per-frame loop:
//     onUpdate()        - before command recording
//     onRender()        - inside the render pass
//     onImGui()         - after ImGui::NewFrame()
//   onCleanup()         - before destruction
class IVkSubRenderer {
public:
    virtual ~IVkSubRenderer() = default;
    virtual void onInit(VulkanContext& /*ctx*/, const VulkanCommandPool& /*pool*/,
                        VkRenderPass /*renderPass*/, uint32_t /*framesInFlight*/) {}
    virtual void onUpdate(uint32_t /*frameIndex*/) {}
    virtual void onRender(VkCommandBuffer /*cmd*/, uint32_t /*frameIndex*/) {}
    virtual void onCleanup(VkDevice /*device*/) {}
    virtual void onImGui() {}
};

// UI panel registered with VkAppBase::add(); onImGui() is called once per frame.
class IVkUIPanel {
public:
    virtual ~IVkUIPanel() = default;
    virtual void onImGui() = 0;
};

} // namespace VKG
