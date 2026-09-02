#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "IPostEffect.h"
#include "Internal/FullscreenEffectPipeline.h"
#include "../../CGLib/VulkanGraphics/VulkanOffscreen.h"
#include "../../CGLib/VulkanGraphics/VulkanSampler.h"

#include <vector>

namespace Phantom::PostProcess {

// Dual-pass Kawase bloom: bright-pass extract -> N same-resolution Kawase blur iterations
// (ping-ponging between two offscreen targets) -> additive composite with the original HDR
// input. Runs at the chain's native resolution (no down/upsample mip chain); see the
// "実装上の注意事項" note in internal design notes for why a resolution pyramid was
// deferred -- it's a pure performance optimization, not a correctness requirement.
class BloomEffect : public IPostEffect {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> extractFragSpv;
        std::vector<uint32_t> blurFragSpv;
        std::vector<uint32_t> compositeFragSpv;
    };

    struct Params {
        float threshold  = 1.0f;  // luma above which pixels bloom
        float strength    = 0.2f; // composite blend weight
        int   iterations  = 4;    // Kawase blur pass count (clamped to [1, kMaxIterations])
    };

    static constexpr int kMaxIterations = 8;

    void setShaders(Shaders s) { shaders_ = std::move(s); }
    void setParams(const Params& p) { params_ = p; }
    const Params& params() const { return params_; }

    void init(const PostEffectContext& ctx, VkImageView inputView, VkSampler inputSampler) override;
    void apply(VkCommandBuffer cmd, uint32_t frameIndex) override;
    VkImageView  getOutputView()    const override { return composite_.getColorImageView(); }
    VkSampler    getOutputSampler() const override { return compositeSampler_.get(); }
    void destroy(VkDevice device) override;

private:
    struct ExtractUBO { float threshold; float _pad[3] = {}; };
    struct BlurUBO    { glm::vec2 texelSize; float offset; float _pad = 0.f; };
    struct CompositeUBO { float strength; float _pad[3] = {}; };

    Shaders shaders_;
    Params  params_;

    const Phantom::VKG::VulkanContext* ctx_ = nullptr;
    VkImageView originalInputView_    = VK_NULL_HANDLE;
    VkSampler   originalInputSampler_ = VK_NULL_HANDLE;

    Phantom::VKG::VulkanOffscreen extract_;
    Phantom::VKG::VulkanOffscreen blurA_;
    Phantom::VKG::VulkanOffscreen blurB_;
    Phantom::VKG::VulkanOffscreen composite_;
    Phantom::VKG::VulkanSampler   extractSampler_;
    Phantom::VKG::VulkanSampler   blurASampler_;
    Phantom::VKG::VulkanSampler   blurBSampler_;
    Phantom::VKG::VulkanSampler   compositeSampler_;

    detail::FullscreenEffectPipeline extractPipeline_;
    detail::FullscreenEffectPipeline blurPipeline_;
    detail::FullscreenEffectPipeline compositePipeline_;
};

} // namespace Phantom::PostProcess
