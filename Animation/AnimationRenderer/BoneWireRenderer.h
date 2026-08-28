#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "CGLib/VkAppBase/IVkSubRenderer.h"
#include "CGLib/Renderer/VkRenderer/VkLineRenderer.h"
#include "CGLib/Animation/Animation/Skeleton.h"

#include <optional>
#include <vector>

namespace Phantom::Animation {

// Draws bone wireframe (parent-to-child lines) using VkLineRenderer.
// Also draws X/Y/Z axis lines at origin.
class BoneWireRenderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    void setShaders(Shaders s)   { shaders_ = std::move(s); }
    void setExtent(VkExtent2D e) { extent_ = e; }
    void setVisible(bool v)      { visible_ = v; }
    bool isVisible() const       { return visible_; }

    // Call every frame with the current global transforms and MVP matrix.
    void updatePose(const Skeleton& skeleton,
                    const std::vector<glm::mat4>& globalTransforms,
                    const glm::mat4& mvp);

    // IVkSubRenderer
    void onInit(::VKG::VulkanContext& ctx,
                const ::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass,
                uint32_t framesInFlight) override;

    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

private:
    Shaders    shaders_;
    VkExtent2D extent_  = {1280, 720};
    bool       visible_ = true;
    bool       dirty_   = false;

    const ::VKG::VulkanContext*     ctx_  = nullptr;
    const ::VKG::VulkanCommandPool* pool_ = nullptr;

    std::optional<VKG::VkLineRenderer> lineRenderer_;

    std::vector<float>    positions_;
    std::vector<float>    colors_;
    std::vector<uint32_t> indices_;
    glm::mat4             mvp_{1.f};
};

} // namespace Phantom::Animation
