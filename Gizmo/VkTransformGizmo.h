#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "IVkSubRenderer.h"
#include "VulkanBuffer.h"
#include "VulkanDescriptorPool.h"
#include "VulkanPipeline.h"

#include "GizmoGeometry.h"

#include <array>
#include <optional>
#include <vector>

namespace Phantom::Gizmo {

enum class GizmoMode { Translate, Rotate, Scale };

// Transform of the manipulated object (mirrors CGStudio::Transform)
struct GizmoTransform {
    std::array<float, 3> translation      = {0.f, 0.f, 0.f};
    std::array<float, 3> rotationEulerDeg = {0.f, 0.f, 0.f};
    std::array<float, 3> scale            = {1.f, 1.f, 1.f};
};

struct GizmoMouseState {
    glm::vec2 pos          = {0.f, 0.f};
    bool      leftDown     = false;
    bool      leftPressed  = false;
    bool      leftReleased = false;
};

// GizmoCameraUBO — matches gizmo.vert set=0 binding=0
struct GizmoCameraUBO {
    glm::mat4 view;
    glm::mat4 proj;            // Vulkan Y-flip applied
    glm::vec3 camPos; float _pad0  = 0.f;
    float     vpWidth  = 0.f;
    float     vpHeight = 0.f;
    float     _pad1    = 0.f, _pad2 = 0.f;
};
static_assert(sizeof(GizmoCameraUBO) == 160, "GizmoCameraUBO layout mismatch");

// GizmoPushConstants — matches gizmo.vert / gizmo.frag push_constant (80 bytes)
struct GizmoPushConstants {
    glm::mat4 model;
    glm::vec4 color;
};
static_assert(sizeof(GizmoPushConstants) == 80, "GizmoPushConstants layout mismatch");


class VkTransformGizmo : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    VkTransformGizmo() = default;
    VkTransformGizmo(const VkTransformGizmo&) = delete;
    VkTransformGizmo& operator=(const VkTransformGizmo&) = delete;

    // --- Configuration ---
    void setShaders(Shaders s) { shaders_ = std::move(s); }
    void setMode(GizmoMode m) { mode_ = m; }
    GizmoMode getMode() const { return mode_; }

    // Call each frame before onUpdate (view/proj must be Vulkan Y-flip applied)
    void setCamera(const glm::mat4& view, const glm::mat4& proj,
                   const glm::vec3& camPos, VkExtent2D viewport);

    // nullptr = hide gizmo
    void setTransform(const GizmoTransform* t) { target_ = t; }

    // --- Interaction (steps 6-9) ---
    std::optional<GizmoTransform> processInput(const GizmoMouseState& ms);

    bool isDragging() const { return drag_.active; }
    bool isHot()      const { return hot_; }

    // --- IVkSubRenderer ---
    void onInit   (Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                   VkRenderPass renderPass, uint32_t framesInFlight) override;
    void onUpdate (uint32_t frameIndex) override;
    void onRender (VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

private:
    // Camera state (written by setCamera)
    glm::mat4  view_   {1.f};
    glm::mat4  proj_   {1.f};
    glm::vec3  camPos_ {0.f};
    VkExtent2D extent_ = {1280, 720};

    const GizmoTransform* target_ = nullptr;
    GizmoMode             mode_   = GizmoMode::Translate;

    // Interaction state
    int  hoveredAxis_ = -1;
    bool hot_         = false;

    struct DragState {
        bool      active       = false;
        int       axis         = -1;
        glm::vec3 planeNormal  = {0.f, 1.f, 0.f};
        glm::vec3 planeOrigin  = {0.f, 0.f, 0.f};
        glm::vec3 startHit     = {0.f, 0.f, 0.f};
        float     startAngle   = 0.f;
        GizmoTransform startTransform;
        glm::vec2 startMouse   = {0.f, 0.f};
    } drag_;

    // Vulkan resources
    uint32_t framesInFlight_ = 2;

    Phantom::VKG::VulkanDescriptorSetLayout descLayout_;
    Phantom::VKG::VulkanDescriptorPool      descPool_;
    std::vector<VkDescriptorSet>   descSets_;
    std::vector<Phantom::VKG::VulkanBuffer> cameraUBOs_;

    Phantom::VKG::VulkanPipeline pipeline_;
    Phantom::VKG::VulkanBuffer   vertexBuffer_;
    Phantom::VKG::VulkanBuffer   indexBuffer_;

    Shaders shaders_;

    // Draw ranges (set in onInit from buildGizmoBuffers)
    DrawRange translateRange_;
    DrawRange rotateRange_;
    DrawRange scaleRange_;

    // Returns world-space scale that keeps gizmo at a constant screen size
    float computeGizmoScale() const;

    // Model matrix for the given axis (0=X, 1=Y, 2=Z), already placed at target
    glm::mat4 axisModelMatrix(int axis) const;

    // Returns the axis color, or hover color when that axis == hoveredAxis_
    glm::vec4 axisColor(int axis) const;
};

} // namespace Phantom::Gizmo
