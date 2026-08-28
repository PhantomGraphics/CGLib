#include "BloomEffect.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"

#include <algorithm>

namespace Phantom::PostProcess {

namespace {
constexpr VkFormat kBloomColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kDepthFormat      = VK_FORMAT_D32_SFLOAT; // unused, VulkanOffscreen requires one
}

void BloomEffect::init(const PostEffectContext& ctx, VkImageView inputView, VkSampler inputSampler)
{
    ctx_ = ctx.ctx;
    originalInputView_    = inputView;
    originalInputSampler_ = inputSampler;

    VkDevice device = ctx_->getDevice();
    const uint32_t w = ctx.extent.width, h = ctx.extent.height;

    extract_.create  (*ctx_, w, h, kBloomColorFormat, kDepthFormat);
    blurA_.create    (*ctx_, w, h, kBloomColorFormat, kDepthFormat);
    blurB_.create    (*ctx_, w, h, kBloomColorFormat, kDepthFormat);
    composite_.create(*ctx_, w, h, kBloomColorFormat, kDepthFormat);

    extractSampler_.create  (device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    blurASampler_.create    (device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    blurBSampler_.create    (device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    compositeSampler_.create(device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    // --- Extract: bright-pass threshold, one draw per real frame -> UBO indexed by frameIndex ---
    {
        detail::FullscreenEffectPipelineConfig cfg;
        cfg.vertSpv           = shaders_.vertSpv;
        cfg.fragSpv           = shaders_.extractFragSpv;
        cfg.framesInFlight    = ctx.framesInFlight;
        cfg.uboSize           = sizeof(ExtractUBO);
        cfg.sampledImageCount = 1;
        extractPipeline_.create(*ctx_, extract_.getRenderPass(), cfg);
        for (uint32_t f = 0; f < ctx.framesInFlight; ++f)
            extractPipeline_.setSampledImage(device, f, 0, originalInputView_, originalInputSampler_);
    }

    // --- Blur: Kawase ping-pong, up to kMaxIterations draws per real frame.
    // The 3 possible source images (extract_, blurA_, blurB_) never change identity across
    // the app's lifetime, so their descriptor bindings are set ONCE here (slot 0/1/2) and
    // never rewritten; only the per-iteration offset (pushed via push constant, not a UBO)
    // varies within a frame. See FullscreenEffectPipelineConfig::pushConstantSize for why a
    // UBO would be unsafe here. ---
    {
        detail::FullscreenEffectPipelineConfig cfg;
        cfg.vertSpv           = shaders_.vertSpv;
        cfg.fragSpv           = shaders_.blurFragSpv;
        cfg.framesInFlight    = 3; // repurposed as 3 fixed source slots, not real frame count
        cfg.uboSize           = 0;
        cfg.sampledImageCount = 1;
        cfg.pushConstantSize  = sizeof(BlurUBO);
        blurPipeline_.create(*ctx_, blurA_.getRenderPass(), cfg);
        blurPipeline_.setSampledImage(device, 0, 0, extract_.getColorImageView(), extractSampler_.get());
        blurPipeline_.setSampledImage(device, 1, 0, blurA_.getColorImageView(),   blurASampler_.get());
        blurPipeline_.setSampledImage(device, 2, 0, blurB_.getColorImageView(),   blurBSampler_.get());
    }

    // --- Composite: additive blend of original HDR + blurred bloom, one draw per real frame ---
    {
        detail::FullscreenEffectPipelineConfig cfg;
        cfg.vertSpv           = shaders_.vertSpv;
        cfg.fragSpv           = shaders_.compositeFragSpv;
        cfg.framesInFlight    = ctx.framesInFlight;
        cfg.uboSize           = sizeof(CompositeUBO);
        cfg.sampledImageCount = 2; // 0 = original scene, 1 = blurred bloom (rebound each apply())
        compositePipeline_.create(*ctx_, composite_.getRenderPass(), cfg);
        for (uint32_t f = 0; f < ctx.framesInFlight; ++f)
            compositePipeline_.setSampledImage(device, f, 0, originalInputView_, originalInputSampler_);
    }
}

void BloomEffect::apply(VkCommandBuffer cmd, uint32_t frameIndex)
{
    VkDevice device = ctx_->getDevice();
    VkExtent2D extent = extract_.getExtent();
    glm::vec2 texelSize(1.0f / static_cast<float>(extent.width), 1.0f / static_cast<float>(extent.height));

    // --- Extract ---
    ExtractUBO extractUbo{ params_.threshold, {} };
    extractPipeline_.updateUBO(frameIndex, &extractUbo, sizeof(extractUbo));
    extract_.beginRenderPass(cmd, { 0.f, 0.f, 0.f, 1.f }, 1.0f);
    extractPipeline_.draw(cmd, frameIndex);
    extract_.endRenderPass(cmd);

    // --- Blur (Kawase ping-pong) ---
    const int iterations = std::clamp(params_.iterations, 1, kMaxIterations);
    Phantom::VKG::VulkanOffscreen* dst = &blurA_;
    uint32_t srcSlot = 0; // 0 = extract_
    Phantom::VKG::VulkanOffscreen* lastWritten = &blurA_;
    for (int i = 0; i < iterations; ++i) {
        BlurUBO push{ texelSize, static_cast<float>(i + 1), 0.f };
        dst->beginRenderPass(cmd, { 0.f, 0.f, 0.f, 1.f }, 1.0f);
        blurPipeline_.draw(cmd, srcSlot, &push, sizeof(push));
        dst->endRenderPass(cmd);

        lastWritten = dst;
        if (dst == &blurA_) { dst = &blurB_; srcSlot = 1; }
        else                { dst = &blurA_; srcSlot = 2; }
    }
    VkImageView finalBlurView    = lastWritten->getColorImageView();
    VkSampler   finalBlurSampler = (lastWritten == &blurA_) ? blurASampler_.get() : blurBSampler_.get();

    // --- Composite ---
    // Rebinding this frame's descriptor right before its single draw is safe: unlike the
    // blur loop above, composite only draws once per frame, so there is no earlier draw in
    // this command buffer that could observe a stale binding.
    compositePipeline_.setSampledImage(device, frameIndex, 1, finalBlurView, finalBlurSampler);
    CompositeUBO compositeUbo{ params_.strength, {} };
    compositePipeline_.updateUBO(frameIndex, &compositeUbo, sizeof(compositeUbo));
    composite_.beginRenderPass(cmd, { 0.f, 0.f, 0.f, 1.f }, 1.0f);
    compositePipeline_.draw(cmd, frameIndex);
    composite_.endRenderPass(cmd);
}

void BloomEffect::destroy(VkDevice device)
{
    extractPipeline_.destroy(device);
    blurPipeline_.destroy(device);
    compositePipeline_.destroy(device);

    extractSampler_.destroy(device);
    blurASampler_.destroy(device);
    blurBSampler_.destroy(device);
    compositeSampler_.destroy(device);

    if (ctx_) {
        extract_.destroy(*ctx_);
        blurA_.destroy(*ctx_);
        blurB_.destroy(*ctx_);
        composite_.destroy(*ctx_);
    }
    ctx_ = nullptr;
}

} // namespace Phantom::PostProcess
