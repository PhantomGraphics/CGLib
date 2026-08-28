#pragma once

#include "../../VkAppBase/IVkSubRenderer.h"
#include "VolumePipeline.h"
#include "World.h"

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace VkVolumeView {

class SparseVolumeRenderer : public ::VKG::IVkSubRenderer {
public:
    struct CameraState {
        float azimuth = 0.0f;
        float elevation = 30.0f;
        float distance = 50.0f;
    };

    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    void setWorld(World* world) { world_ = world; }
    void setExtent(VkExtent2D ext) { extent_ = ext; }
    void markDirty() { dirty_ = true; }
    void setShaders(Shaders shaders) { shaders_ = std::move(shaders); }
    void setEnabled(bool e) { enabled_ = e; }

    void handleMouseButton(bool pressed);
    void handleMouseMove(double x, double y);
    void handleScroll(double yOffset);
    void resetCamera() { azimuth_ = 0.f; elevation_ = 30.f; distance_ = 50.f; }

    float getPointSize() const { return pointSize_; }
    void  setPointSize(float s) { pointSize_ = std::clamp(s, 1.0f, 20.0f); }

    CameraState getCameraState() const;

    void onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass, uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;
    void onImGui() override;

private:
    glm::mat4 computeMVP() const;
    void rebuildVertices();

    World* world_ = nullptr;
    const Phantom::VKG::VulkanContext* ctx_ = nullptr;
    const Phantom::VKG::VulkanCommandPool* pool_ = nullptr;

    VkExtent2D extent_{1280, 720};
    uint32_t framesInFlight_ = 2;

    bool dirty_ = true;
    bool dragging_ = false;
    bool enabled_ = true;

    float pointSize_ = 4.0f;
    float azimuth_ = 0.0f;
    float elevation_ = 30.0f;
    float distance_ = 50.0f;

    double prevMouseX_ = 0.0;
    double prevMouseY_ = 0.0;

    Shaders shaders_;
    VolumePipeline pipeline_;
    Phantom::VKG::VulkanBuffer vertexBuffer_;
    std::vector<PointVertex> vertices_;
};

} // namespace VkVolumeView
