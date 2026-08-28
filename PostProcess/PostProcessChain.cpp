#include "PostProcessChain.h"

namespace Phantom::PostProcess {

void PostProcessChain::addEffect(std::unique_ptr<IPostEffect> effect)
{
    effects_.push_back(std::move(effect));
}

void PostProcessChain::init(const PostEffectContext& ctx, VkImageView hdrView, VkSampler hdrSampler)
{
    passthroughView_    = hdrView;
    passthroughSampler_ = hdrSampler;

    VkImageView view    = hdrView;
    VkSampler   sampler = hdrSampler;
    for (auto& effect : effects_) {
        effect->init(ctx, view, sampler);
        view    = effect->getOutputView();
        sampler = effect->getOutputSampler();
    }
}

void PostProcessChain::apply(VkCommandBuffer cmd, uint32_t frameIndex)
{
    for (auto& effect : effects_)
        effect->apply(cmd, frameIndex);
}

VkImageView PostProcessChain::getFinalView() const
{
    return effects_.empty() ? passthroughView_ : effects_.back()->getOutputView();
}

VkSampler PostProcessChain::getFinalSampler() const
{
    return effects_.empty() ? passthroughSampler_ : effects_.back()->getOutputSampler();
}

void PostProcessChain::destroy(VkDevice device)
{
    for (auto& effect : effects_)
        effect->destroy(device);
    effects_.clear();
    passthroughView_    = VK_NULL_HANDLE;
    passthroughSampler_ = VK_NULL_HANDLE;
}

} // namespace Phantom::PostProcess
