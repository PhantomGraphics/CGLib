#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "Volume3DImage.h"
#include "../Volume/VolumeScattering.h"

#include "../../VulkanGraphics/VulkanBuffer.h"
#include "../../VulkanGraphics/VulkanComputePipeline.h"
#include "../../VulkanGraphics/VulkanDescriptorPool.h"
#include "../../VulkanGraphics/VulkanPipeline.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::Volume {

/**
 * @brief GPU single-scattering raymarcher over a density 3D image
 * (docs/todo/SPEC_volume_raymarch.md). Mirrors VolumeScattering.cpp, the CPU reference.
 *
 * Owns the density and sun-transmittance images, a compute pass that fills the latter from the
 * former, and a fullscreen pass that raymarches into the current render pass with premultiplied
 * output (rgb = radiance, a = 1 - transmittance). The density image can be filled by upload()
 * or written by another compute pass (density()); this class never assumes cloud-specific data.
 */
class VolumeRaymarchGpu {
public:
    struct Config {
        std::vector<uint32_t> fullscreenVertSpv;
        std::vector<uint32_t> raymarchFragSpv;
        std::vector<uint32_t> sunTransmittanceCompSpv;
        VkRenderPass renderPass = VK_NULL_HANDLE;   ///< Pass the raymarch draws into. Null = compute only.
        uint32_t framesInFlight = 2;
    };

    struct Camera {
        glm::mat4 invViewProj = glm::mat4(1.0f);    ///< Vulkan depth range 0..1.
        glm::vec3 position = glm::vec3(0.0f);
    };

    VolumeRaymarchGpu() = default;
    VolumeRaymarchGpu(const VolumeRaymarchGpu&) = delete;
    VolumeRaymarchGpu& operator=(const VolumeRaymarchGpu&) = delete;

    bool create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                const Config& config);
    /** @brief The device must be idle. */
    void destroy(const Phantom::VKG::VulkanContext& ctx);

    /** @brief (Re)creates the grid images; the device must be idle. Contents become zero / undefined. */
    bool setGrid(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                 const ScalarGridDesc& desc);
    bool uploadDensity(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                       const ScalarGrid3D& density) const;

    /** @brief Fills the sun-transmittance image from the density image (call when either or the sun changes). */
    void recordSunTransmittance(VkCommandBuffer cmd, const ScatteringParams& params) const;

    /**
     * @brief Raymarches into the active render pass. `tMax` > 0 limits every ray (opaque depth is
     * not sampled yet). frameIndex selects the per-frame uniform buffer.
     */
    void recordRaymarch(VkCommandBuffer cmd, uint32_t frameIndex, const Camera& camera,
                        const ScatteringParams& params, float tMax = 0.0f);

    const Volume3DImage& density() const { return density_; }
    const Volume3DImage& sunTransmittance() const { return sunT_; }
    const ScalarGridDesc& gridDesc() const { return desc_; }
    VkSampler densitySampler() const { return zeroBorderSampler_; }         ///< border 0
    VkSampler transmittanceSampler() const { return oneBorderSampler_; }    ///< border 1
    bool hasGrid() const { return density_.isValid(); }
    bool canRaymarch() const { return raymarchPipeline_.getPipeline() != VK_NULL_HANDLE; }

private:
    struct Params;   // std140 UBO, see the .cpp
    void updateDescriptors(VkDevice device);

    ScalarGridDesc desc_;
    Volume3DImage density_;
    Volume3DImage sunT_;
    VkSampler zeroBorderSampler_ = VK_NULL_HANDLE;   // clamp-to-border, outside = 0 (density)
    VkSampler oneBorderSampler_ = VK_NULL_HANDLE;    // clamp-to-border, outside = 1 (sun transmittance)

    Phantom::VKG::VulkanDescriptorSetLayout sunSetLayout_;
    Phantom::VKG::VulkanDescriptorSetLayout marchSetLayout_;
    Phantom::VKG::VulkanDescriptorPool pool_;
    VkDescriptorSet sunSet_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> marchSets_;
    std::vector<Phantom::VKG::VulkanBuffer> ubos_;
    Phantom::VKG::VulkanComputePipeline sunPipeline_;
    Phantom::VKG::VulkanPipeline raymarchPipeline_;
};

} // namespace Phantom::Volume
