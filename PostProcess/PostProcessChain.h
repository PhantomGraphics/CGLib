#pragma once

#include "IPostEffect.h"

#include <memory>
#include <vector>

namespace Phantom::PostProcess {

// Owns an ordered list of IPostEffect stages and wires each one's input to the previous
// stage's output. Typical use (see docs/todo/PLAN_universe_app.md Phase E):
//
//   PostProcessChain chain;
//   chain.addEffect(std::make_unique<BloomEffect>(...));
//   chain.addEffect(std::make_unique<ToneMappingEffect>(...));
//   chain.addEffect(std::make_unique<FXAAEffect>(...));
//   chain.init(ctx, hdrOffscreen.getColorImageView(), hdrSampler);
//   ...
//   chain.apply(cmd, frameIndex);
//   blitToSwapchain(chain.getFinalView());
class PostProcessChain {
public:
    PostProcessChain() = default;
    PostProcessChain(const PostProcessChain&) = delete;
    PostProcessChain& operator=(const PostProcessChain&) = delete;

    // Effects must be added before init(). Ownership transfers to the chain.
    void addEffect(std::unique_ptr<IPostEffect> effect);

    // Calls init() on every added effect in order, chaining input->output as it goes.
    // hdrView/hdrSampler feed the first effect; a chain with no effects makes
    // getFinalView()/getFinalSampler() return hdrView/hdrSampler unchanged (pass-through).
    void init(const PostEffectContext& ctx, VkImageView hdrView, VkSampler hdrSampler);

    // Records every effect's apply() in order.
    void apply(VkCommandBuffer cmd, uint32_t frameIndex);

    VkImageView getFinalView()    const;
    VkSampler   getFinalSampler() const;

    bool empty() const { return effects_.empty(); }

    void destroy(VkDevice device);

private:
    std::vector<std::unique_ptr<IPostEffect>> effects_;
    VkImageView passthroughView_    = VK_NULL_HANDLE;
    VkSampler   passthroughSampler_ = VK_NULL_HANDLE;
};

} // namespace Phantom::PostProcess
