#pragma once

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanPipeline.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>

namespace VkVolumeView {

struct PointVertex {
    glm::vec3 pos;
    glm::vec4 color;
};

class VolumePipeline {
public:
    struct UBO {
        glm::mat4 mvp;
        float pointSize = 4.0f;
        float _pad0 = 0.0f;
        float _pad1 = 0.0f;
        float _pad2 = 0.0f;
    };

    void create(const Phantom::VKG::VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
                std::vector<uint32_t> vertSpv, std::vector<uint32_t> fragSpv);
    void destroy(VkDevice device);
    void updateUBO(uint32_t frameIndex, const UBO& ubo);

    VkPipeline getPipeline() const { return pipeline_.getPipeline(); }
    VkPipelineLayout getLayout() const { return pipeline_.getLayout(); }
    VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const { return sets_[frameIndex]; }

private:
    Phantom::VKG::VulkanDescriptorSetLayout dsl_;
    Phantom::VKG::VulkanDescriptorPool pool_;
    std::vector<VkDescriptorSet> sets_;
    Phantom::VKG::VulkanPipeline pipeline_;
    std::vector<Phantom::VKG::VulkanBuffer> ubos_;
};

} // namespace VkVolumeView
