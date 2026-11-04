#include "PostProcessChain.h"
#include "CGLib/VulkanGraphics/VulkanContext.h"

namespace Phantom::Effects::PostProcess {

void PostProcessChain::addEffect(std::unique_ptr<IPostEffect> effect)
{
    if (!effect || initialized_) return;
    effects_.push_back(std::move(effect));
}

bool PostProcessChain::init(const PostEffectContext& ctx, VkImageView hdrView, VkSampler hdrSampler)
{
    if (initialized_ || !ctx.ctx || !ctx.ctx->getDevice() || !hdrView || !hdrSampler
        || !ctx.extent.width || !ctx.extent.height || !ctx.framesInFlight) return false;
    passthroughView_    = hdrView;
    passthroughSampler_ = hdrSampler;

    VkImageView view    = hdrView;
    VkSampler   sampler = hdrSampler;
    for (auto& effect : effects_) {
        if (!effect->init(ctx, view, sampler)) {
            for (auto& stage : effects_) stage->destroy(ctx.ctx->getDevice());
            passthroughView_ = VK_NULL_HANDLE;
            passthroughSampler_ = VK_NULL_HANDLE;
            return false;
        }
        view    = effect->getOutputView();
        sampler = effect->getOutputSampler();
    }
    initialized_ = true;
    return true;
}

void PostProcessChain::apply(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!initialized_) return;
    for (auto& effect : effects_)
        effect->apply(cmd, frameIndex);
}

VkImageView PostProcessChain::getFinalView() const
{
    if (!initialized_) return VK_NULL_HANDLE;
    return effects_.empty() ? passthroughView_ : effects_.back()->getOutputView();
}

VkSampler PostProcessChain::getFinalSampler() const
{
    if (!initialized_) return VK_NULL_HANDLE;
    return effects_.empty() ? passthroughSampler_ : effects_.back()->getOutputSampler();
}

void PostProcessChain::destroy(VkDevice device)
{
    for (auto& effect : effects_)
        effect->destroy(device);
    effects_.clear();
    initialized_ = false;
    passthroughView_    = VK_NULL_HANDLE;
    passthroughSampler_ = VK_NULL_HANDLE;
}

} // namespace Phantom::Effects::PostProcess
