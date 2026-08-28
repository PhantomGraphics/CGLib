#include "MeshOverlayRenderer.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

#include <glm/gtc/matrix_transform.hpp>

namespace VkVolumeView {

void MeshOverlayRenderer::onInit(Phantom::VKG::VulkanContext& ctx,
                                    const Phantom::VKG::VulkanCommandPool& pool,
                                    VkRenderPass renderPass,
                                    uint32_t framesInFlight)
{
    ctx_  = &ctx;
    pool_ = &pool;

    Phantom::VKG::VkTriangleRenderer::Config cfg;
    cfg.vertSpv     = std::move(shaders_.vertSpv);
    cfg.fragSpv     = std::move(shaders_.fragSpv);
    cfg.blendEnable = false;
    cfg.cullBack    = false;

    renderer_ = std::make_unique<Phantom::VKG::VkTriangleRenderer>(std::move(cfg));
    renderer_->create(ctx, pool, renderPass, framesInFlight);

    dirty_ = true;
}

void MeshOverlayRenderer::onUpdate(uint32_t frameIndex) {
    if (!ctx_ || !pool_ || !renderer_) return;

    if (dirty_) {
        rebuildMesh();
        dirty_ = false;
    }

    renderer_->updateMVP(frameIndex, computeMVP());
}

void MeshOverlayRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (!renderer_ || !renderer_->isValid()) return;
    if (buffer_.indices.empty()) return;
    renderer_->render(cmd, frameIndex);
}

void MeshOverlayRenderer::onCleanup(VkDevice device) {
    if (renderer_) renderer_->destroy(device);
    renderer_.reset();
}

glm::mat4 MeshOverlayRenderer::computeMVP() const {
    const float az = glm::radians(camera_.azimuth);
    const float el = glm::radians(camera_.elevation);

    const glm::vec3 target(0.f, 0.f, 0.f);
    const glm::vec3 eye(
        camera_.distance * std::cos(el) * std::sin(az),
        camera_.distance * std::sin(el),
        camera_.distance * std::cos(el) * std::cos(az));

    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.f, 1.f, 0.f));

    const float aspect = (extent_.height > 0)
        ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
        : 1.0f;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 1000.0f);
    proj[1][1] *= -1.0f;

    return proj * view;
}

void MeshOverlayRenderer::rebuildMesh() {
    buffer_ = {};

    if (!world_ || !ctx_ || !pool_ || !renderer_) return;

    for (const auto& mesh : world_->getPolygons()) {
        if (!mesh.visible) continue;

        const uint32_t baseVertex = static_cast<uint32_t>(buffer_.positions.size() / 3);

        buffer_.positions.insert(buffer_.positions.end(),
                                 mesh.positions.begin(), mesh.positions.end());
        buffer_.colors.insert(buffer_.colors.end(),
                               mesh.colors.begin(), mesh.colors.end());

        for (const uint32_t idx : mesh.indices)
            buffer_.indices.push_back(baseVertex + idx);
    }

    if (buffer_.indices.empty()) return;

    renderer_->upload(*ctx_, *pool_, buffer_);
}

} // namespace VkVolumeView
