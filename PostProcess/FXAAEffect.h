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

// Lightweight luma-edge-detection FXAA. Operates on an LDR input (expects to run after
// ToneMappingEffect) and computes luma inline rather than relying on a precomputed alpha
// channel, so it is a simplified pass rather than the full NVIDIA FXAA 3.11 reference.
class FXAAEffect : public IPostEffect {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    struct Params {
        float contrastThreshold = 0.0833f; // minimum local contrast to treat as an edge
        float relativeThreshold = 0.125f;  // contrast relative to local max luma
        float subpixelBlend     = 0.75f;   // 0 = off, 1 = full subpixel aliasing removal
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
        glm::vec2 texelSize;
        float contrastThreshold;
        float relativeThreshold;
        float subpixelBlend;
        float _pad[3] = {};
    };

    Shaders shaders_;
    Params  params_;

    const Phantom::VKG::VulkanContext* ctx_ = nullptr;

    Phantom::VKG::VulkanOffscreen    output_;
    Phantom::VKG::VulkanSampler      outputSampler_;
    detail::FullscreenEffectPipeline pipeline_;
};

} // namespace Phantom::PostProcess
