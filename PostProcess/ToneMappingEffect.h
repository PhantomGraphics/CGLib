#pragma once

#include "IPostEffect.h"
#include "Internal/FullscreenEffectPipeline.h"
#include "../../CGLib/VulkanGraphics/VulkanOffscreen.h"
#include "../../CGLib/VulkanGraphics/VulkanSampler.h"

#include <vector>

namespace Phantom::PostProcess {

// ACES Filmic tone mapping (Hill 2015 fit) + gamma 2.2 + optional vignette.
// Converts the HDR input to an LDR (UNORM) output; typically the second-to-last stage in
// the chain, followed only by FXAAEffect.
class ToneMappingEffect : public IPostEffect {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    struct Params {
        float exposure         = 1.0f;
        bool  vignetteEnabled  = false;
        float vignetteStrength = 0.5f; // 0 = no darkening, 1 = strong corner falloff
    };

    void setShaders(Shaders s) { shaders_ = std::move(s); }
    void setParams(const Params& p) { params_ = p; }
    const Params& params() const { return params_; }

    void init(const PostEffectContext& ctx, VkImageView inputView, VkSampler inputSampler) override;
    void apply(VkCommandBuffer cmd, uint32_t frameIndex) override;
    VkImageView  getOutputView()    const override { return output_.getColorImageView(); }
    VkSampler    getOutputSampler() const override { return outputSampler_.get(); }
    void destroy(VkDevice device) override;

private:
    struct UBO {
        float exposure;
        int32_t vignetteEnabled;
        float vignetteStrength;
        float _pad = 0.f;
    };

    Shaders shaders_;
    Params  params_;

    const Phantom::VKG::VulkanContext* ctx_ = nullptr;

    Phantom::VKG::VulkanOffscreen        output_;
    Phantom::VKG::VulkanSampler          outputSampler_;
    detail::FullscreenEffectPipeline pipeline_;
};

} // namespace Phantom::PostProcess
