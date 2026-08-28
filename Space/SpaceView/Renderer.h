#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "../../CGLib/Renderer/VkRenderer/VkLineRenderer.h"
#include "../../CGLib/Renderer/VkRenderer/VkPointRenderer.h"
#include "World.h"

#include <optional>
#include <vector>

namespace VKSpace {

/// @brief Sub-renderer that draws line wireframes and point clouds from World.
///
/// Owns a VkLineRenderer (for spatial structure wireframes) and a VkPointRenderer
/// (for SignedDistance sample points).  On each frame, if World::SpaceResult::dirty
/// is set the buffers are uploaded to the GPU.
class Renderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> lineVert;
        std::vector<uint32_t> lineFrag;
        std::vector<uint32_t> pointVert;
        std::vector<uint32_t> pointFrag;
    };

    explicit Renderer(World* world);

    /// @brief Update swapchain extent used for the MVP aspect ratio.
    /// Call from VkSpaceApp::onInit() and onSwapChainCreated().
    void setExtent(VkExtent2D ext) { extent_ = ext; }
    void setShaders(Shaders shaders) { shaders_ = std::move(shaders); }

    // Mouse / camera input forwarded from window callbacks.
    void handleMouseButton(bool leftPressed);
    void handleMouseMove(double x, double y);
    void handleScroll(double dy);
    void resetCamera();
    float getCameraDistance() const { return camDist_; }

    // IVkSubRenderer
    void onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass, uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

private:
    World*     world_;
    VkExtent2D extent_ = {1280, 720};

    // Camera (spherical coordinates)
    float     camTheta_   = 1.0f;
    float     camPhi_     = 0.5f;
    float     camDist_    = 4.0f;
    glm::vec3 camTarget_  = {0.f, 0.f, 0.f};
    double    lastX_      = 0.;
    double    lastY_      = 0.;
    bool      isDragging_ = false;

    // Vulkan resources (non-owning refs set in onInit)
    const Phantom::VKG::VulkanContext*     ctx_  = nullptr;
    const Phantom::VKG::VulkanCommandPool* pool_ = nullptr;

    Shaders shaders_;

    // Constructed lazily in onInit once SPIR-V is loaded.
    std::optional<Phantom::VKG::VkLineRenderer>  lineRenderer_;
    std::optional<Phantom::VKG::VkPointRenderer> pointRenderer_;

    std::vector<float>    axisPositions_;
    std::vector<float>    axisColors_;
    std::vector<uint32_t> axisIndices_;
    bool                  hasLineGeometry_ = false;

    glm::mat4 computeMVP() const;

    void buildAxisGeometry();
    void uploadBuffers();
};

} // namespace VKSpace
