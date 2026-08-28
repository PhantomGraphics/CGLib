#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::PostProcess {

// Shared setup context passed to IPostEffect::init(). extent/framesInFlight describe the
// chain's working resolution and how many frames the caller keeps in flight for per-frame
// resources (UBOs, descriptor sets). The offscreen color targets owned by each effect are
// NOT per-frame-in-flight arrays -- they are single instances re-rendered every frame, the
// same simplifying assumption already used by Physics/FluidRenderer's SSFR offscreen chain
// (see SSFROffscreenSet.h: "Each pass runs sequentially so framesInFlight is always 1").
struct PostEffectContext {
    const Phantom::VKG::VulkanContext*     ctx  = nullptr;
    const Phantom::VKG::VulkanCommandPool* pool = nullptr;
    VkExtent2D extent         = { 1280, 720 };
    uint32_t   framesInFlight = 2;
};

// One stage in a PostProcessChain. Implementations own their output render target(s)
// (Phantom::VKG::VulkanOffscreen) and rebuild their descriptor's input binding whenever the
// upstream view changes (PostProcessChain::init() wires init()'s inputView/inputSampler to
// the previous effect's getOutputView()/getOutputSampler()).
class IPostEffect {
public:
    virtual ~IPostEffect() = default;

    // Called once by PostProcessChain::init(). inputView/inputSampler is the previous
    // effect's output (or the chain's original HDR source for the first effect).
    virtual void init(const PostEffectContext& ctx, VkImageView inputView, VkSampler inputSampler) = 0;

    // Records this effect's draw commands. Must not be called before init().
    virtual void apply(VkCommandBuffer cmd, uint32_t frameIndex) = 0;

    virtual VkImageView getOutputView()    const = 0;
    virtual VkSampler   getOutputSampler() const = 0;

    virtual void destroy(VkDevice device) = 0;
};

} // namespace Phantom::PostProcess
