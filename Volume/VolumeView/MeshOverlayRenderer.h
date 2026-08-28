#pragma once

#include "../../VkAppBase/IVkSubRenderer.h"
#include "SparseVolumeRenderer.h"
#include "World.h"

#include "../../../CGLib/Renderer/VkRenderer/VkTriangleRenderer.h"
#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace VkVolumeView {

// IVkSubRenderer that renders PolygonMesh overlays stored in VolumeWorld.
// Wraps VKG::VkTriangleRenderer, which is an IVkRenderer (not IVkSubRenderer).
class MeshOverlayRenderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    void setWorld(World* world)                        { world_  = world; }
    void setExtent(VkExtent2D ext)                           { extent_ = ext; }
    void markDirty()                                         { dirty_  = true; }
    void syncCamera(const SparseVolumeRenderer::CameraState& cam) { camera_ = cam; }
    void setShaders(Shaders shaders)                         { shaders_ = std::move(shaders); }

    void onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass, uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

private:
    glm::mat4 computeMVP() const;
    void rebuildMesh();

    World*  world_  = nullptr;
    const Phantom::VKG::VulkanContext*     ctx_  = nullptr;
    const Phantom::VKG::VulkanCommandPool* pool_ = nullptr;

    VkExtent2D extent_{1280, 720};
    bool dirty_ = true;

    SparseVolumeRenderer::CameraState camera_;

    Shaders shaders_;
    std::unique_ptr<Phantom::VKG::VkTriangleRenderer> renderer_;

    Phantom::VKG::VkTriangleRenderer::Buffer buffer_;
};

} // namespace VkVolumeView
