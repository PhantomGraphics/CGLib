#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "IPostEffect.h"
#include "Internal/FullscreenEffectPipeline.h"
#include "../../CGLib/VulkanGraphics/VulkanOffscreen.h"
#include "../../CGLib/VulkanGraphics/VulkanSampler.h"

#include <vector>

namespace Phantom::VKG { class VulkanCommandPool; }

namespace Phantom::PostProcess {

// Hemisphere-sampling SSAO (Crysis / LearnOpenGL style) that darkens the chain's color
// input using a caller-supplied view-space G-buffer (linear depth + normal).
//
// This library does not produce that G-buffer itself -- Phase A only builds the
// standalone PostProcess library, and the main scene pass doesn't yet output an MRT normal
// attachment (see docs/todo/PLAN_universe_app.md Phase C/E, and the "不足ライブラリ分析"
// table's "マルチライト" row). Wire setGBuffer() to whatever produces those two views once
// that MRT extension exists; until then, this effect simply has nothing to sample.
class SSAOEffect : public IPostEffect {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> ssaoFragSpv;
        std::vector<uint32_t> blurFragSpv;
    };

    struct Params {
        int   kernelSize = 16;    // clamped to [1, kMaxKernelSize]
        float radius      = 0.5f; // view-space sample radius, world units
        float bias        = 0.025f;
        float strength    = 1.0f; // 0 = no darkening, 1 = full computed AO
    };

    static constexpr int kMaxKernelSize = 32;
    static constexpr int kNoiseDim      = 4; // 4x4 tiled rotation-noise texture

    void setShaders(Shaders s) { shaders_ = std::move(s); }
    void setParams(const Params& p) { params_ = p; }
    const Params& params() const { return params_; }

    // View-space linear depth (R32_SFLOAT) and view-space normal (xyz in RGB, any format
    // with linear sampling) from the caller's G-buffer pass. Must be called before init().
    void setGBuffer(VkImageView depthView, VkSampler depthSampler,
                    VkImageView normalView, VkSampler normalSampler);

    // Projection matrix used to reconstruct view-space position from linear depth + UV.
    // Safe to call every frame (e.g. on resize) before apply().
    void setProjection(const glm::mat4& proj) { proj_ = proj; }

    void init(const PostEffectContext& ctx, VkImageView inputView, VkSampler inputSampler) override;
    void apply(VkCommandBuffer cmd, uint32_t frameIndex) override;
    VkImageView  getOutputView()    const override { return composite_.getColorImageView(); }
    VkSampler    getOutputSampler() const override { return compositeSampler_.get(); }
    void destroy(VkDevice device) override;

private:
    struct SSAOUBO {
        glm::mat4 proj;
        glm::vec4 kernel[kMaxKernelSize]; // xyz = tangent-space sample offset
        glm::vec2 noiseScale;
        int32_t   kernelSize;
        float     radius;
        float     bias;
        float     _pad[3] = {};
    };
    struct BlurCompositeUBO {
        glm::vec2 texelSize;
        float     strength;
        float     _pad = 0.f;
    };

    Shaders   shaders_;
    Params    params_;
    glm::mat4 proj_ = glm::mat4(1.f);

    VkImageView depthView_     = VK_NULL_HANDLE;
    VkSampler   depthSampler_  = VK_NULL_HANDLE;
    VkImageView normalView_    = VK_NULL_HANDLE;
    VkSampler   normalSampler_ = VK_NULL_HANDLE;

    const Phantom::VKG::VulkanContext* ctx_ = nullptr;
    VkImageView originalInputView_    = VK_NULL_HANDLE;
    VkSampler   originalInputSampler_ = VK_NULL_HANDLE;

    Phantom::VKG::VulkanOffscreen raw_;       // R8_UNORM raw AO factor
    Phantom::VKG::VulkanOffscreen composite_; // final color * blurred AO
    Phantom::VKG::VulkanSampler   rawSampler_;
    Phantom::VKG::VulkanSampler   compositeSampler_;

    detail::FullscreenEffectPipeline ssaoPipeline_;
    detail::FullscreenEffectPipeline blurPipeline_;

    // Noise texture: kNoiseDim x kNoiseDim, RG16F tangent-space rotation vectors, generated
    // once from a fixed seed (deterministic -- keeps scenario-test screenshots reproducible).
    VkImage        noiseImage_  = VK_NULL_HANDLE;
    VkDeviceMemory noiseMemory_ = VK_NULL_HANDLE;
    VkImageView    noiseView_   = VK_NULL_HANDLE;
    Phantom::VKG::VulkanSampler noiseSampler_;
    glm::vec4 kernel_[kMaxKernelSize]{};

    void createKernelAndNoise(const Phantom::VKG::VulkanCommandPool& pool);
};

} // namespace Phantom::PostProcess
