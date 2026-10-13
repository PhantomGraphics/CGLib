#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "../../CGLib/VkAppBase/IVkSubRenderer.h"
#include "../../CGLib/Renderer/VkRenderer/VkLineRenderer.h"
#include "../../CGLib/Renderer/VkRenderer/VkPointRenderer.h"
#include "../../CGLib/Renderer/VkRenderer/VkTriangleRenderer.h"
#include "MeshWorld.h"

#include <optional>
#include <vector>

namespace MeshView {

/// @brief Draws the mesh of MeshWorld (faces, edges, vertex points) plus the world axes, and owns the orbit camera.
///
/// When MeshWorld::draw() is dirty the buffers are uploaded to the GPU; the camera position is handed to the world each
/// frame so the face-orientation colours follow it.
class Renderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> lineVert, lineFrag;
        std::vector<uint32_t> pointVert, pointFrag;
        std::vector<uint32_t> triVert, triFrag;
    };

    explicit Renderer(MeshWorld* world);

    void setExtent(VkExtent2D ext) { extent_ = ext; }
    void setShaders(Shaders shaders) { shaders_ = std::move(shaders); }

    // Camera input forwarded from the window callbacks.
    void handleMouseButton(bool leftPressed);
    void handleMouseMove(double x, double y);
    void handleScroll(double dy);
    void resetCamera();
    void setCamera(float theta, float phi, float dist);
    float getCameraDistance() const { return camDist_; }
    glm::vec3 eye() const;

    // The camera as MeshEdit's picking sees it (object space = world space; proj carries the Vulkan Y flip).
    Phantom::MeshEdit::MeshPickView pickView() const;

    // IVkSubRenderer
    void onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass, uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

private:
    MeshWorld* world_;
    VkExtent2D extent_ = { 1280, 720 };

    // Camera (spherical coordinates around the origin)
    float camTheta_ = 1.2f;
    float camPhi_ = 0.7f;
    float camDist_ = 3.2f;
    glm::vec3 camTarget_ = { 0.f, 0.f, 0.f };
    double lastX_ = 0., lastY_ = 0.;
    bool isDragging_ = false;

    const Phantom::VKG::VulkanContext* ctx_ = nullptr;
    const Phantom::VKG::VulkanCommandPool* pool_ = nullptr;

    Shaders shaders_;
    std::optional<Phantom::VKG::VkLineRenderer> lineRenderer_;
    std::optional<Phantom::VKG::VkPointRenderer> pointRenderer_;
    std::optional<Phantom::VKG::VkTriangleRenderer> triRenderer_;

    std::vector<float> axisPositions_, axisColors_;
    std::vector<uint32_t> axisIndices_;
    bool hasLines_ = false, hasPoints_ = false, hasTriangles_ = false;

    glm::mat4 computeView() const;
    glm::mat4 computeProj() const;
    void buildAxisGeometry();
    void uploadBuffers(MeshDraw& draw);
};

}  // namespace MeshView
