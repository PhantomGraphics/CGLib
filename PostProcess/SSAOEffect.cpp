#include "SSAOEffect.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../CGLib/VulkanGraphics/VulkanImage.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace Phantom::PostProcess {

namespace {
constexpr VkFormat kHdrFormat   = VK_FORMAT_R16G16B16A16_SFLOAT; // composite output (matches HDR chain)
constexpr VkFormat kAoFormat    = VK_FORMAT_R8_UNORM;            // raw AO factor
constexpr VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;          // unused, VulkanOffscreen requires one
}

void SSAOEffect::setGBuffer(VkImageView depthView, VkSampler depthSampler,
                            VkImageView normalView, VkSampler normalSampler)
{
    depthView_     = depthView;
    depthSampler_  = depthSampler;
    normalView_    = normalView;
    normalSampler_ = normalSampler;
}

void SSAOEffect::createKernelAndNoise(const Phantom::VKG::VulkanCommandPool& pool)
{
    // --- Hemisphere kernel: samples biased towards the origin (LearnOpenGL SSAO tutorial
    // scheme) so most samples land close to the fragment being shaded. Fixed seed keeps
    // scenario-test screenshots reproducible across runs. ---
    std::mt19937 rng(0xA0AEu);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> signedUnit(-1.0f, 1.0f);

    for (int i = 0; i < kMaxKernelSize; ++i) {
        glm::vec3 s(signedUnit(rng), signedUnit(rng), unit(rng)); // hemisphere around +Z
        s = glm::normalize(s) * unit(rng);
        float scale = static_cast<float>(i) / static_cast<float>(kMaxKernelSize);
        scale = 0.1f + 0.9f * scale * scale; // lerp(0.1, 1.0, scale^2)
        kernel_[i] = glm::vec4(s * scale, 0.f);
    }

    // --- Noise texture: kNoiseDim x kNoiseDim tangent-space rotation vectors (RG, Z=0),
    // tiled across the screen to randomize the kernel's per-pixel orientation. Stored as
    // R8G8_SNORM (2 bytes/texel) so no half-float packing is needed. ---
    constexpr int kTexelCount = kNoiseDim * kNoiseDim;
    std::vector<int8_t> snormPixels(kTexelCount * 2);
    for (int i = 0; i < kTexelCount; ++i) {
        glm::vec2 n = glm::normalize(glm::vec2(signedUnit(rng), signedUnit(rng)));
        snormPixels[i * 2 + 0] = static_cast<int8_t>(n.x * 127.0f);
        snormPixels[i * 2 + 1] = static_cast<int8_t>(n.y * 127.0f);
    }

    VkDevice device = ctx_->getDevice();
    constexpr VkFormat kNoiseFormat = VK_FORMAT_R8G8_SNORM;

    // Upload through the shared helper: on failure it leaves null handles and nothing allocated,
    // and the effect simply has no noise texture (the descriptor write below sees a null view).
    if (!Phantom::VKG::VulkanImage::createFromPixels(*ctx_, pool,
            reinterpret_cast<const uint8_t*>(snormPixels.data()), 2, kNoiseDim, kNoiseDim, kNoiseFormat, false,
            noiseImage_, noiseMemory_, noiseView_))
        std::fprintf(stderr, "[SSAOEffect] noise texture upload failed\n");
    noiseSampler_.create(device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT);
}

void SSAOEffect::init(const PostEffectContext& ctx, VkImageView inputView, VkSampler inputSampler)
{
    ctx_ = ctx.ctx;
    originalInputView_    = inputView;
    originalInputSampler_ = inputSampler;

    VkDevice device = ctx_->getDevice();
    const uint32_t w = ctx.extent.width, h = ctx.extent.height;

    raw_.create      (*ctx_, w, h, kAoFormat,  kDepthFormat);
    composite_.create(*ctx_, w, h, kHdrFormat, kDepthFormat);
    rawSampler_.create      (device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    compositeSampler_.create(device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    createKernelAndNoise(*ctx.pool);

    // --- SSAO pass: binding 0 = UBO, 1 = depth, 2 = normal, 3 = noise ---
    {
        detail::FullscreenEffectPipelineConfig cfg;
        cfg.vertSpv           = shaders_.vertSpv;
        cfg.fragSpv           = shaders_.ssaoFragSpv;
        cfg.framesInFlight    = ctx.framesInFlight;
        cfg.uboSize           = sizeof(SSAOUBO);
        cfg.sampledImageCount = 3;
        ssaoPipeline_.create(*ctx_, raw_.getRenderPass(), cfg);
        for (uint32_t f = 0; f < ctx.framesInFlight; ++f) {
            ssaoPipeline_.setSampledImage(device, f, 0, depthView_, depthSampler_);
            ssaoPipeline_.setSampledImage(device, f, 1, normalView_, normalSampler_);
            ssaoPipeline_.setSampledImage(device, f, 2, noiseView_, noiseSampler_.get());
        }
    }

    // --- Blur + composite: binding 0 = UBO, 1 = raw AO, 2 = original scene color ---
    {
        detail::FullscreenEffectPipelineConfig cfg;
        cfg.vertSpv           = shaders_.vertSpv;
        cfg.fragSpv           = shaders_.blurFragSpv;
        cfg.framesInFlight    = ctx.framesInFlight;
        cfg.uboSize           = sizeof(BlurCompositeUBO);
        cfg.sampledImageCount = 2;
        blurPipeline_.create(*ctx_, composite_.getRenderPass(), cfg);
        for (uint32_t f = 0; f < ctx.framesInFlight; ++f) {
            blurPipeline_.setSampledImage(device, f, 0, raw_.getColorImageView(), rawSampler_.get());
            blurPipeline_.setSampledImage(device, f, 1, originalInputView_, originalInputSampler_);
        }
    }
}

void SSAOEffect::apply(VkCommandBuffer cmd, uint32_t frameIndex)
{
    VkExtent2D extent = raw_.getExtent();
    const int kernelSize = std::clamp(params_.kernelSize, 1, kMaxKernelSize);

    // --- SSAO ---
    SSAOUBO ssaoUbo{};
    ssaoUbo.proj = proj_;
    std::memcpy(ssaoUbo.kernel, kernel_, sizeof(kernel_));
    ssaoUbo.noiseScale = glm::vec2(static_cast<float>(extent.width)  / static_cast<float>(kNoiseDim),
                                   static_cast<float>(extent.height) / static_cast<float>(kNoiseDim));
    ssaoUbo.kernelSize = kernelSize;
    ssaoUbo.radius     = params_.radius;
    ssaoUbo.bias       = params_.bias;
    ssaoPipeline_.updateUBO(frameIndex, &ssaoUbo, sizeof(ssaoUbo));

    raw_.beginRenderPass(cmd, { 1.f, 1.f, 1.f, 1.f }, 1.0f);
    ssaoPipeline_.draw(cmd, frameIndex);
    raw_.endRenderPass(cmd);

    // --- Blur + composite ---
    BlurCompositeUBO blurUbo{};
    blurUbo.texelSize = glm::vec2(1.0f / static_cast<float>(extent.width),
                                  1.0f / static_cast<float>(extent.height));
    blurUbo.strength  = params_.strength;
    blurPipeline_.updateUBO(frameIndex, &blurUbo, sizeof(blurUbo));

    composite_.beginRenderPass(cmd, { 0.f, 0.f, 0.f, 1.f }, 1.0f);
    blurPipeline_.draw(cmd, frameIndex);
    composite_.endRenderPass(cmd);
}

void SSAOEffect::destroy(VkDevice device)
{
    ssaoPipeline_.destroy(device);
    blurPipeline_.destroy(device);

    rawSampler_.destroy(device);
    compositeSampler_.destroy(device);
    noiseSampler_.destroy(device);

    if (noiseView_)   { vkDestroyImageView(device, noiseView_,   nullptr); noiseView_   = VK_NULL_HANDLE; }
    if (noiseImage_)  { vkDestroyImage(device, noiseImage_,      nullptr); noiseImage_  = VK_NULL_HANDLE; }
    if (noiseMemory_) { vkFreeMemory(device, noiseMemory_,       nullptr); noiseMemory_ = VK_NULL_HANDLE; }

    if (ctx_) {
        raw_.destroy(*ctx_);
        composite_.destroy(*ctx_);
    }
    ctx_ = nullptr;
}

} // namespace Phantom::PostProcess
