#include "VkTransformGizmo.h"

#include "VulkanContext.h"
#include "VulkanCommandPool.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>
#include <stdexcept>

namespace Phantom::Gizmo {

// ─── Configuration ────────────────────────────────────────────────────────────

void VkTransformGizmo::setCamera(const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& camPos, VkExtent2D viewport)
{
    view_   = view;
    proj_   = proj;
    camPos_ = camPos;
    extent_ = viewport;
}

// ─── IVkSubRenderer ───────────────────────────────────────────────────────────

void VkTransformGizmo::onInit(Phantom::VKG::VulkanContext& ctx,
                               const Phantom::VKG::VulkanCommandPool& pool,
                               VkRenderPass renderPass,
                               uint32_t framesInFlight)
{
    framesInFlight_ = framesInFlight;
    VkDevice device = ctx.getDevice();

    // 1. Build CPU geometry, upload to device-local VBO/IBO
    auto geo       = buildGizmoBuffers();
    translateRange_ = geo.translate;
    rotateRange_    = geo.rotate;
    scaleRange_     = geo.scale;

    vertexBuffer_.create(ctx, pool,
        geo.vertices.size() * sizeof(GizmoVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        geo.vertices.data());

    indexBuffer_.create(ctx, pool,
        geo.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        geo.indices.data());

    // 2. Descriptor set layout — binding 0: GizmoCameraUBO (vertex stage)
    {
        VkDescriptorSetLayoutBinding b{};
        b.binding         = 0;
        b.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;
        descLayout_.create(device, { b });
    }

    // 3. Descriptor pool + per-frame sets
    {
        VkDescriptorPoolSize ps{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight };
        descPool_.create(device, { ps }, framesInFlight);

        std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descLayout_.get());
        descSets_ = descPool_.allocateSets(device, layouts);
    }

    // 4. Per-frame camera UBOs — write descriptor sets
    cameraUBOs_.resize(framesInFlight);
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        cameraUBOs_[i].createMapped(ctx, sizeof(GizmoCameraUBO),
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        VkDescriptorBufferInfo bi{ cameraUBOs_[i].getBuffer(), 0, sizeof(GizmoCameraUBO) };
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = descSets_[i];
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &bi;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
    }

    // 5. Graphics pipeline
    {
        // Vertex format: binding 0, stride = sizeof(GizmoVertex) = 12 bytes, pos only
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(GizmoVertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attr{};
        attr.location = 0;
        attr.binding  = 0;
        attr.format   = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset   = 0;

        // Push constants: vertex + fragment, 80 bytes (mat4 model + vec4 color)
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcRange.offset     = 0;
        pcRange.size       = sizeof(GizmoPushConstants);

        Phantom::VKG::PipelineConfig cfg{};
        cfg.vertSpv             = std::move(shaders_.vertSpv);
        cfg.fragSpv             = std::move(shaders_.fragSpv);
        cfg.bindingDescs        = { binding };
        cfg.attrDescs           = { attr };
        cfg.topology            = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        cfg.descriptorSetLayout = descLayout_.get();
        cfg.cullMode            = VK_CULL_MODE_NONE;
        cfg.depthTest           = false;  // gizmo always in front
        cfg.depthWrite          = false;
        cfg.pushConstantRanges  = { pcRange };

        pipeline_.create(ctx, renderPass, cfg);
    }
}

void VkTransformGizmo::onUpdate(uint32_t frameIndex)
{
    if (frameIndex >= cameraUBOs_.size()) return;

    GizmoCameraUBO ubo{};
    ubo.view     = view_;
    ubo.proj     = proj_;
    ubo.camPos   = camPos_;
    ubo.vpWidth  = static_cast<float>(extent_.width);
    ubo.vpHeight = static_cast<float>(extent_.height);

    cameraUBOs_[frameIndex].write(&ubo, sizeof(ubo));
}

void VkTransformGizmo::onRender(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!target_)                        return; // hidden
    if (!pipeline_.getPipeline())        return; // not initialized
    if (frameIndex >= descSets_.size())  return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkBuffer     vbuf   = vertexBuffer_.get();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.get(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1,
                            &descSets_[frameIndex], 0, nullptr);

    const DrawRange& range =
        (mode_ == GizmoMode::Translate) ? translateRange_ :
        (mode_ == GizmoMode::Rotate)    ? rotateRange_    :
                                          scaleRange_;

    for (int axis = 0; axis < 3; ++axis) {
        GizmoPushConstants pc{};
        pc.model = axisModelMatrix(axis);
        pc.color = axisColor(axis);
        vkCmdPushConstants(cmd, pipeline_.getLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), &pc);
        vkCmdDrawIndexed(cmd, range.indexCount, 1, range.firstIndex, 0, 0);
    }
}

void VkTransformGizmo::onCleanup(VkDevice device)
{
    pipeline_.destroy(device);

    for (auto& ub : cameraUBOs_) ub.destroy(device);
    cameraUBOs_.clear();

    descPool_.destroy(device);
    descLayout_.destroy(device);

    vertexBuffer_.destroy(device);
    indexBuffer_.destroy(device);
}

// ─── Interaction (steps 6-9) ──────────────────────────────────────────────────

std::optional<GizmoTransform> VkTransformGizmo::processInput(const GizmoMouseState& ms)
{
    if (!target_) {
        hoveredAxis_ = -1;
        hot_         = false;
        drag_.active = false;
        return std::nullopt;
    }

    glm::vec3 center = {
        target_->translation[0],
        target_->translation[1],
        target_->translation[2]
    };
    float s = computeGizmoScale();

    // Unit-length axis directions in world space
    static const glm::vec3 kAxisDir[3] = { {1,0,0}, {0,1,0}, {0,0,1} };

    // ── Helpers ────────────────────────────────────────────────────────────

    // World → screen pixel
    auto w2s = [this](const glm::vec3& world) -> glm::vec2 {
        glm::vec4 clip = proj_ * view_ * glm::vec4(world, 1.f);
        if (clip.w <= 0.f) return { -1e6f, -1e6f };
        float sx = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(extent_.width);
        float sy = (clip.y / clip.w * 0.5f + 0.5f) * static_cast<float>(extent_.height);
        return { sx, sy };
    };

    // 2D point-to-segment distance
    auto ptSeg = [](glm::vec2 p, glm::vec2 a, glm::vec2 b) -> float {
        glm::vec2 ab  = b - a;
        float     len2 = glm::dot(ab, ab);
        if (len2 < 1e-6f) return glm::length(p - a);
        float t = glm::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f);
        return glm::length(p - (a + t * ab));
    };

    // Screen pixel → normalised world-space ray direction
    auto screenRay = [this](glm::vec2 sp) -> glm::vec3 {
        float ndcX =  2.f * sp.x / static_cast<float>(extent_.width)  - 1.f;
        float ndcY = -(2.f * sp.y / static_cast<float>(extent_.height) - 1.f);
        glm::mat4 invVP = glm::inverse(proj_ * view_);
        glm::vec4 n4 = invVP * glm::vec4(ndcX, ndcY, 0.f, 1.f); // near (depth=0)
        glm::vec4 f4 = invVP * glm::vec4(ndcX, ndcY, 1.f, 1.f); // far  (depth=1)
        glm::vec3 nearPt = glm::vec3(n4) / n4.w;
        glm::vec3 farPt  = glm::vec3(f4) / f4.w;
        return glm::normalize(farPt - nearPt);
    };

    // Ray-plane intersection (returns false if nearly parallel)
    auto rayPlane = [](glm::vec3 ro, glm::vec3 rd,
                       glm::vec3 pn, glm::vec3 pp, glm::vec3& hit) -> bool {
        float denom = glm::dot(pn, rd);
        if (std::fabs(denom) < 1e-5f) return false;
        float t = glm::dot(pn, pp - ro) / denom;
        if (t < 0.f) return false;
        hit = ro + t * rd;
        return true;
    };

    // ── Drag start ─────────────────────────────────────────────────────────
    if (ms.leftPressed && hoveredAxis_ >= 0 && !drag_.active) {
        drag_.active         = true;
        drag_.axis           = hoveredAxis_;
        drag_.startTransform = *target_;
        drag_.startMouse     = ms.pos;

        if (mode_ == GizmoMode::Translate) {
            // Drag plane: contains axisDir, tilted to face camera as much as possible
            const glm::vec3 axd   = kAxisDir[drag_.axis];
            glm::vec3       toCam = glm::normalize(camPos_ - center);
            glm::vec3       perp  = toCam - glm::dot(toCam, axd) * axd;
            if (glm::length(perp) < 1e-4f)
                perp = glm::cross(axd, glm::vec3{0.f, 1.f, 0.f});
            drag_.planeNormal = glm::normalize(perp);
            drag_.planeOrigin = center;
            // Record hit point at press
            glm::vec3 ray = screenRay(ms.pos);
            if (!rayPlane(camPos_, ray, drag_.planeNormal, drag_.planeOrigin, drag_.startHit))
                drag_.startHit = center; // fallback
        }
        else if (mode_ == GizmoMode::Rotate) {
            drag_.startAngle = drag_.startTransform.rotationEulerDeg[drag_.axis];
        }
        // Scale: no extra setup needed
    }

    // ── Drag release ───────────────────────────────────────────────────────
    if (ms.leftReleased)
        drag_.active = false;

    // ── Drag update (each frame while held) ────────────────────────────────
    std::optional<GizmoTransform> result;
    if (drag_.active) {
        GizmoTransform t = drag_.startTransform;

        if (mode_ == GizmoMode::Translate) {
            glm::vec3 ray = screenRay(ms.pos);
            glm::vec3 hit;
            if (rayPlane(camPos_, ray, drag_.planeNormal, drag_.planeOrigin, hit)) {
                float delta = glm::dot(hit - drag_.startHit, kAxisDir[drag_.axis]);
                t.translation[drag_.axis] = drag_.startTransform.translation[drag_.axis] + delta;
            }
        }
        else if (mode_ == GizmoMode::Rotate) {
            // Mouse X movement → angle change (0.5 °/px)
            float deltaDeg = (ms.pos.x - drag_.startMouse.x) * 0.5f;
            t.rotationEulerDeg[drag_.axis] =
                drag_.startTransform.rotationEulerDeg[drag_.axis] + deltaDeg;
        }
        else if (mode_ == GizmoMode::Scale) {
            // Project mouse onto screen-space axis direction and compute ratio
            glm::vec2 centerSS  = w2s(center);
            glm::vec2 tipSS     = w2s(center + s * kAxisDir[drag_.axis]);
            glm::vec2 axisScreen = tipSS - centerSS;
            float     axisLen    = glm::length(axisScreen);
            if (axisLen > 1e-3f) {
                axisScreen /= axisLen;
                float projNow   = glm::dot(ms.pos            - centerSS, axisScreen);
                float projStart = glm::dot(drag_.startMouse  - centerSS, axisScreen);
                if (std::fabs(projStart) > 1e-3f) {
                    float factor = glm::max(0.01f, projNow / projStart);
                    t.scale[drag_.axis] = drag_.startTransform.scale[drag_.axis] * factor;
                }
            }
        }
        result = t;
    }

    // ── Hover detection (suppressed while dragging) ─────────────────────────
    if (!drag_.active) {
        glm::vec2   centerSS   = w2s(center);
        constexpr float kHoverR = 8.f;
        hoveredAxis_ = -1;
        float bestDist = kHoverR;
        for (int axis = 0; axis < 3; ++axis) {
            glm::vec2 tipSS = w2s(center + s * kAxisDir[axis]);
            float dist = ptSeg(ms.pos, centerSS, tipSS);
            if (dist < bestDist) { bestDist = dist; hoveredAxis_ = axis; }
        }
    }

    hot_ = drag_.active || (hoveredAxis_ >= 0);
    return result;
}

// ─── Private helpers ──────────────────────────────────────────────────────────

float VkTransformGizmo::computeGizmoScale() const
{
    if (!target_) return 1.f;
    glm::vec3 center = {
        target_->translation[0],
        target_->translation[1],
        target_->translation[2]
    };
    float depth     = glm::length(camPos_ - center);
    float focalLen  = -proj_[1][1]; // Y-flip proj: stored value is negative
    float vpH       = static_cast<float>(extent_.height);
    return (90.f / (focalLen * vpH / 2.f)) * depth;
}

glm::mat4 VkTransformGizmo::axisModelMatrix(int axis) const
{
    glm::vec3 center = target_
        ? glm::vec3{target_->translation[0], target_->translation[1], target_->translation[2]}
        : glm::vec3{0.f};

    float s = computeGizmoScale();
    glm::mat4 T = glm::translate(glm::mat4{1.f}, center);
    glm::mat4 S = glm::scale   (glm::mat4{1.f}, glm::vec3{s});
    glm::mat4 R;
    switch (axis) {
    case 0: R = glm::rotate(glm::mat4{1.f}, glm::radians(+90.f), {0.f, 1.f, 0.f}); break; // +Z -> +X
    case 1: R = glm::rotate(glm::mat4{1.f}, glm::radians(-90.f), {1.f, 0.f, 0.f}); break; // +Z -> +Y
    default: R = glm::mat4{1.f}; break; // +Z stays +Z
    }
    return T * S * R;
}

glm::vec4 VkTransformGizmo::axisColor(int axis) const
{
    if (axis == hoveredAxis_)
        return {1.f, 1.f, 0.f, 1.f}; // yellow hover
    switch (axis) {
    case 0:  return {1.f,  0.2f, 0.2f, 1.f}; // X = red
    case 1:  return {0.2f, 1.f,  0.2f, 1.f}; // Y = green
    default: return {0.2f, 0.5f, 1.f,  1.f}; // Z = blue
    }
}

} // namespace Phantom::Gizmo
