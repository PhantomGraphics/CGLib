#include "SparseVolumeRenderer.h"

#include "imgui.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

#include "../../../CGLib/Graphics/ColorMap.h"
#include "../../../CGLib/Graphics/ColorTable.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace VkVolumeView {

void SparseVolumeRenderer::onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                    VkRenderPass renderPass, uint32_t framesInFlight) {
    ctx_ = &ctx;
    pool_ = &pool;
    framesInFlight_ = framesInFlight;

    pipeline_.create(ctx, renderPass, framesInFlight,
                     std::move(shaders_.vertSpv), std::move(shaders_.fragSpv));
    dirty_ = true;
}

void SparseVolumeRenderer::onUpdate(uint32_t frameIndex) {
    if (!ctx_ || !pool_) {
        return;
    }

    if (dirty_) {
        rebuildVertices();
        dirty_ = false;
    }

    VolumePipeline::UBO ubo{};
    ubo.mvp = computeMVP();
    ubo.pointSize = pointSize_;
    pipeline_.updateUBO(frameIndex, ubo);
}

void SparseVolumeRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (!enabled_ || vertices_.empty() || !vertexBuffer_.isValid()) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkBuffer vbuf = vertexBuffer_.getBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

    const VkDescriptorSet set = pipeline_.getDescriptorSet(frameIndex);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1, &set, 0, nullptr);

    vkCmdDraw(cmd, static_cast<uint32_t>(vertices_.size()), 1, 0, 0);
}

void SparseVolumeRenderer::onCleanup(VkDevice device) {
    vertexBuffer_.destroy(device);
    pipeline_.destroy(device);
    vertices_.clear();
}

void SparseVolumeRenderer::onImGui() {
}

void SparseVolumeRenderer::handleMouseButton(bool pressed) {
    dragging_ = pressed;
}

void SparseVolumeRenderer::handleMouseMove(double x, double y) {
    if (dragging_) {
        const float dx = static_cast<float>(x - prevMouseX_);
        const float dy = static_cast<float>(y - prevMouseY_);
        azimuth_ += dx * 0.4f;
        elevation_ = std::clamp(elevation_ + dy * 0.3f, -85.0f, 85.0f);
    }

    prevMouseX_ = x;
    prevMouseY_ = y;
}

void SparseVolumeRenderer::handleScroll(double yOffset) {
    distance_ = std::clamp(distance_ - static_cast<float>(yOffset) * 0.8f, 1.0f, 500.0f);
}

SparseVolumeRenderer::CameraState SparseVolumeRenderer::getCameraState() const {
    CameraState s{};
    s.azimuth = azimuth_;
    s.elevation = elevation_;
    s.distance = distance_;
    return s;
}

glm::mat4 SparseVolumeRenderer::computeMVP() const {
    const float az = glm::radians(azimuth_);
    const float el = glm::radians(elevation_);

    const glm::vec3 target(0.0f, 0.0f, 0.0f);
    const glm::vec3 eye(
        distance_ * std::cos(el) * std::sin(az),
        distance_ * std::sin(el),
        distance_ * std::cos(el) * std::cos(az));

    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));

    const float aspect = (extent_.height > 0)
        ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
        : 1.0f;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 1000.0f);
    proj[1][1] *= -1.0f;

    return proj * view;
}

void SparseVolumeRenderer::rebuildVertices() {
    vertices_.clear();

    if (!world_ || !ctx_ || !pool_) {
        return;
    }

    Phantom::Graphics::ColorMap colorMap(0.0f, 30.0f, Phantom::Graphics::ColorTable::createJetTable(256));

    const auto& scenes = world_->getScenes();
    for (const auto& scene : scenes) {
        if (!scene || !scene->getShape() || !scene->isVisible()) {
            continue;
        }

        scene->getShape()->forEachActive([&](const Phantom::Volume::Coord&, const Phantom::Math::Vector3df& worldPos, float value) {
            const auto c = colorMap.getInterpolatedColor(value);

            PointVertex v{};
            v.pos = glm::vec3(worldPos.x, worldPos.y, worldPos.z);
            v.color = glm::vec4(c.x, c.y, c.z, 1.0f);
            vertices_.push_back(v);
        });
    }

    vertexBuffer_.destroy(ctx_->getDevice());
    if (vertices_.empty()) {
        return;
    }

    vertexBuffer_.create(*ctx_, *pool_,
                         sizeof(PointVertex) * vertices_.size(),
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vertices_.data());
}

} // namespace VkVolumeView
