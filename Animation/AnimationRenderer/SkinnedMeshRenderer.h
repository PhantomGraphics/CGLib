#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "CGLib/VkAppBase/IVkSubRenderer.h"
#include "CGLib/Renderer/VkRenderer/VkTriangleRenderer.h"
#include "CGLib/Animation/Animation/SkinnedMesh.h"

#include <optional>
#include <vector>

namespace Phantom::Animation {

// Applies CPU skinning to a SkinnedMesh and renders via VkTriangleRenderer.
class SkinnedMeshRenderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    void setShaders(Shaders s)   { shaders_ = std::move(s); }
    void setVisible(bool v)      { visible_ = v; }
    bool isVisible() const       { return visible_; }

    // Supply the mesh once (bind pose).
    void setMesh(SkinnedMesh mesh) { mesh_ = std::move(mesh); }

    // Call every frame with current skin matrices and MVP.
    void updatePose(const std::vector<glm::mat4>& skinMatrices,
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
    bool       visible_ = true;
    bool       dirty_   = false;

    const ::VKG::VulkanContext*     ctx_  = nullptr;
    const ::VKG::VulkanCommandPool* pool_ = nullptr;

    std::optional<VKG::VkTriangleRenderer> triRenderer_;

    SkinnedMesh           mesh_;
    std::vector<glm::mat4> skinMatrices_;
    glm::mat4              mvp_{1.f};

    std::vector<float>    skinnedPositions_;
    std::vector<float>    skinnedColors_;
};

} // namespace Phantom::Animation
