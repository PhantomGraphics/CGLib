#pragma once

#include "../../VkAppBase/IVkSubRenderer.h"
#include "LinePipeline.h"
#include "SparseVolumeRenderer.h"
#include "World.h"

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>

namespace VkVolumeView {

class VectorFieldRenderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    void setWorld(World* world) { world_ = world; }
    void setExtent(VkExtent2D ext) { extent_ = ext; }
    void markDirty() { dirty_ = true; }
    void syncCamera(const SparseVolumeRenderer::CameraState& cam) { camera_ = cam; }
    void setShaders(Shaders shaders) { shaders_ = std::move(shaders); }
    void setEnabled(bool e) { enabled_ = e; }
    void setShowVectorField(bool show);
    void setShowVolumeGrid(bool show);
    bool getShowVectorField() const { return showVectorField_; }
    bool getShowVolumeGrid() const { return showVolumeGrid_; }

    void onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass, uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

private:
    glm::mat4 computeMVP() const;
    void rebuildLines();

    World* world_ = nullptr;
    const Phantom::VKG::VulkanContext* ctx_ = nullptr;
    const Phantom::VKG::VulkanCommandPool* pool_ = nullptr;

    VkExtent2D extent_{1280, 720};

    bool dirty_ = true;
    bool enabled_ = false;
    bool showVectorField_ = true;
    bool showVolumeGrid_ = false;
    float scale_ = 1.0f;

    SparseVolumeRenderer::CameraState camera_;

    Shaders shaders_;
    LinePipeline pipeline_;
    Phantom::VKG::VulkanBuffer vertexBuffer_;
    std::vector<LineVertex> vertices_;
};

} // namespace VkVolumeView
