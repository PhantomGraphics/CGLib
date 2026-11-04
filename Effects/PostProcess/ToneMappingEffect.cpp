#include "ToneMappingEffect.h"
#include "CGLib/VulkanGraphics/VulkanContext.h"

namespace Phantom::Effects::PostProcess {

bool ToneMappingEffect::init(const PostEffectContext& ctx, VkImageView inputView, VkSampler inputSampler)
{
    if (ctx_ || !ctx.ctx || !ctx.ctx->getDevice() || !ctx.extent.width || !ctx.extent.height
        || !ctx.framesInFlight || !inputView || !inputSampler) return false;
    ctx_ = ctx.ctx;

    if (!output_.create(*ctx_, ctx.extent.width, ctx.extent.height,
                   VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D32_SFLOAT)) { destroy(ctx_->getDevice()); return false; }
    if (!outputSampler_.create(ctx_->getDevice(), VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)) { destroy(ctx_->getDevice()); return false; }

    detail::FullscreenEffectPipelineConfig cfg;
    cfg.vertSpv           = shaders_.vertSpv;
    cfg.fragSpv           = shaders_.fragSpv;
    cfg.framesInFlight    = ctx.framesInFlight;
    cfg.uboSize           = sizeof(UBO);
    cfg.sampledImageCount = 1;
    if (!pipeline_.create(*ctx_, output_.getRenderPass(), cfg)) { destroy(ctx_->getDevice()); return false; }

    for (uint32_t f = 0; f < ctx.framesInFlight; ++f)
        pipeline_.setSampledImage(ctx_->getDevice(), f, 0, inputView, inputSampler);
    return true;
}

void ToneMappingEffect::apply(VkCommandBuffer cmd, uint32_t frameIndex)
{
    UBO ubo{ params_.exposure, params_.vignetteEnabled ? 1 : 0, params_.vignetteStrength, 0.f };
    pipeline_.updateUBO(frameIndex, &ubo, sizeof(ubo));

    output_.beginRenderPass(cmd, { 0.f, 0.f, 0.f, 1.f }, 1.0f);
    pipeline_.draw(cmd, frameIndex);
    output_.endRenderPass(cmd);
}

void ToneMappingEffect::destroy(VkDevice device)
{
    pipeline_.destroy(device);
    outputSampler_.destroy(device);
    if (ctx_) output_.destroy(*ctx_);
    ctx_ = nullptr;
}

} // namespace Phantom::Effects::PostProcess
