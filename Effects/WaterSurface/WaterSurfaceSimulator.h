#pragma once
#include "WaterSurfaceReference.h"
#include "CGLib/VulkanGraphics/VulkanBuffer.h"
#include "CGLib/VulkanGraphics/VulkanComputePipeline.h"
#include "CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include <array>

namespace Phantom::Effects::WaterSurface {
// Record in submission order on a single graphics queue, outside render passes.
// Recorded updates must be submitted exactly once. Caller waits before destroy.
class WaterSurfaceSimulator {
public:
    bool create(const VKG::VulkanContext&, const VKG::VulkanCommandPool&,
                const WaterSurfaceConfig&, const std::vector<uint32_t>& updateSpv);
    void destroy(VkDevice);
    bool addImpulse(const WaterImpulse&);
    void requestReset() { reset_ = true; impulses_.clear(); accumulator_ = 0; }
    void recordUpdate(VkCommandBuffer, double elapsed, bool paused = false, bool singleStep = false);
    VkBuffer buffer() const { return states_[current_].get(); }
    const WaterSurfaceConfig& config() const { return config_; }
    bool isValid() const { return pipeline_.isValid(); }
private:
    struct Push {
        uint32_t columns, rows, operation, unused;
        float width, depth, speed, damping;
        float dt, x, z, radius;
        float velocity, padding[3];
    };
    static_assert(sizeof(Push) == 64);
    void dispatch(VkCommandBuffer, const Push&);
    std::array<VKG::VulkanBuffer, 2> states_;
    VKG::VulkanDescriptorSetLayout layout_;
    VKG::VulkanDescriptorPool pool_;
    VKG::VulkanComputePipeline pipeline_;
    std::vector<VkDescriptorSet> sets_;
    std::vector<WaterImpulse> impulses_;
    WaterSurfaceConfig config_;
    uint32_t current_ = 0;
    double accumulator_ = 0, step_ = 1.0/120;
    bool reset_ = false;
};
}
