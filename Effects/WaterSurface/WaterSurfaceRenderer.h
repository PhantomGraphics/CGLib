#pragma once
#include "WaterSurfaceSimulator.h"
#include "CGLib/VulkanGraphics/VulkanPipeline.h"
#include <glm/glm.hpp>

namespace Phantom::Effects::WaterSurface {
struct WaterSurfaceCamera {
    glm::mat4 viewProjection{1}, inverseViewProjection{1}, reflectionViewProjection{1};
    glm::vec4 eye{0,5,5,1};
};
struct WaterSurfaceMaterial {
    glm::vec4 color{0.02f,0.22f,0.28f,1};
    // RGB absorption per world unit; w controls screen-space distortion.
    glm::vec4 absorption{0.3f,0.12f,0.06f,0.025f};
    bool reflection = true, refraction = true;
};
struct WaterSurfaceImages {
    VkDescriptorImageInfo reflection{}, color{}, depth{};
};
class WaterSurfaceRenderer {
public:
    bool create(const VKG::VulkanContext&, VkRenderPass, uint32_t frames,
                const WaterSurfaceConfig&, const std::vector<uint32_t>& vert,
                const std::vector<uint32_t>& frag);
    void destroy(VkDevice);
    // Call after this frame's fence, after simulator.recordUpdate(). Images must
    // remain alive until submission completes; depth uses Vulkan's [0,1] range.
    void update(VkDevice, uint32_t frame, VkBuffer state, const WaterSurfaceImages&,
                const WaterSurfaceCamera&, const WaterSurfaceMaterial&, float elevation = 0);
    void recordDraw(VkCommandBuffer, uint32_t frame);
private:
    struct Uniform {
        glm::mat4 vp, inverseVP, reflectionVP;
        glm::vec4 eye, domain, color, absorption, flags;
        glm::uvec4 grid;
    };
    static_assert(sizeof(Uniform) == 288);
    VKG::VulkanPipeline pipeline_;
    VKG::VulkanDescriptorPool pool_;
    VKG::VulkanDescriptorSetLayout layout_;
    std::vector<VKG::VulkanBuffer> uniforms_;
    std::vector<VkDescriptorSet> sets_;
    WaterSurfaceConfig config_;
};
}
