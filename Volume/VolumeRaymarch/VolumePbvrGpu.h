#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "VolumeRaymarchGpu.h"

#include "../../VulkanGraphics/VulkanBuffer.h"
#include "../../VulkanGraphics/VulkanComputePipeline.h"
#include "../../VulkanGraphics/VulkanDescriptorPool.h"
#include "../../VulkanGraphics/VulkanOffscreen.h"
#include "../../VulkanGraphics/VulkanPipeline.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace Phantom::Volume {

/**
 * @brief Point-based volume rendering (PBVR) of the same density / sun-transmittance grids that
 * VolumeRaymarchGpu marches (docs/todo/SPEC_volume_raymarch.md). Each ensemble draws opaque discs
 * with a Poisson count per cell so that sigma = n * a / V; the mean of the ensembles converges to
 * the raymarch's premultiplied (L, 1 - T). Order independent: opaque + depth test only.
 *
 * Per ensemble (all outside a render pass, in recordEnsembles()):
 *   fill draw args -> generate (compute) -> draw discs into the ensemble target -> add into the
 *   accumulation image. recordComposite() then blends accum / count inside the caller's pass.
 * A cell that would need more than maxPerCell particles gets larger discs instead of being cut
 * (statistically exact, only blurrier); running out of the global capacity is counted in stats().
 */
class VolumePbvrGpu {
public:
    struct Config {
        std::vector<uint32_t> generateCompSpv, pointVertSpv, pointFragSpv, accumulateCompSpv;
        std::vector<uint32_t> fullscreenVertSpv, compositeFragSpv;
        VkRenderPass compositeRenderPass = VK_NULL_HANDLE;   ///< Pass recordComposite() draws into.
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;         ///< Depth format of the ensemble target.
        uint32_t framesInFlight = 2;
        uint32_t particleCapacity = 1u << 20;
        float minDiameterPx = 1.5f;
        float maxPerCell = 16.0f;
    };

    struct Camera {
        glm::mat4 viewProj = glm::mat4(1.0f);   ///< Cloud space -> clip.
        glm::vec3 position = glm::vec3(0.0f);   ///< Cloud space.
        float pixelAngle = 0.001f;              ///< Radians per pixel.
        float projScalePx = 500.0f;             ///< Pixels per unit of clip.w at unit world size.
    };

    struct Stats {
        uint32_t overflowed = 0;   ///< Particles dropped because the capacity was reached (since reset).
        uint32_t generated = 0;    ///< Particles drawn (since reset).
    };

    VolumePbvrGpu() = default;
    VolumePbvrGpu(const VolumePbvrGpu&) = delete;
    VolumePbvrGpu& operator=(const VolumePbvrGpu&) = delete;

    bool create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                const Config& config);
    /** @brief The device must be idle. */
    void destroy(const Phantom::VKG::VulkanContext& ctx);

    /** @brief (Re)creates the screen-sized targets; the device must be idle. Accumulation restarts. */
    bool setViewport(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                     uint32_t width, uint32_t height);
    /** @brief Points the descriptors at the grid images (call after VolumeRaymarchGpu::setGrid()). */
    void bindGrid(const Phantom::VKG::VulkanContext& ctx, const VolumeRaymarchGpu& grid);

    /** @brief Forget the accumulated ensembles (density, sun, camera or parameters changed). */
    void resetAccumulation() { accumulated_ = 0; resetStats_ = true; }
    uint32_t accumulatedEnsembles() const { return accumulated_; }

    /** @brief Renders `count` more ensembles into the accumulation image (outside a render pass). */
    void recordEnsembles(VkCommandBuffer cmd, uint32_t frameIndex, const VolumeRaymarchGpu& grid,
                         const Camera& camera, const ScatteringParams& params, uint32_t count);
    /** @brief Blends the ensemble average into the active render pass (premultiplied). */
    void recordComposite(VkCommandBuffer cmd) const;

    /** @brief Blocking readback of the counters (tests / diagnostics). */
    bool readStats(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                   Stats& out) const;

    bool isReady() const { return pointPipeline_.getPipeline() != VK_NULL_HANDLE && accumImage_ != VK_NULL_HANDLE; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }

private:
    struct Params;   // std140 UBO, see the .cpp
    void destroyTargets(const Phantom::VKG::VulkanContext& ctx);

    Config config_;
    uint32_t width_ = 0, height_ = 0;
    uint32_t accumulated_ = 0;
    uint32_t ensembleCounter_ = 0;   // monotonically increasing RNG stream index
    bool resetStats_ = true;

    Phantom::VKG::VulkanOffscreen ensembleTarget_;
    VkImage accumImage_ = VK_NULL_HANDLE;
    VkDeviceMemory accumMemory_ = VK_NULL_HANDLE;
    VkImageView accumView_ = VK_NULL_HANDLE;
    VkSampler nearestSampler_ = VK_NULL_HANDLE;

    Phantom::VKG::VulkanBuffer particles_, drawArgs_, stats_;
    std::vector<Phantom::VKG::VulkanBuffer> ubos_;

    Phantom::VKG::VulkanDescriptorSetLayout genLayout_, accumLayout_, compositeLayout_;
    Phantom::VKG::VulkanDescriptorPool descriptorPool_;
    std::vector<VkDescriptorSet> genSets_;   // per frame in flight
    VkDescriptorSet accumSet_ = VK_NULL_HANDLE;
    VkDescriptorSet compositeSet_ = VK_NULL_HANDLE;

    Phantom::VKG::VulkanComputePipeline generatePipeline_, accumulatePipeline_;
    Phantom::VKG::VulkanPipeline pointPipeline_, compositePipeline_;
};

} // namespace Phantom::Volume
