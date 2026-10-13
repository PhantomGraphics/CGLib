#include "Renderer.h"

#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace MeshView {

Renderer::Renderer(MeshWorld* world) : world_(world) { buildAxisGeometry(); }

// ---- camera ----

void Renderer::handleMouseButton(bool leftPressed) { isDragging_ = leftPressed; }

void Renderer::handleMouseMove(double x, double y) {
    if (isDragging_) {
        const float dx = static_cast<float>(x - lastX_) * 0.005f;
        const float dy = static_cast<float>(y - lastY_) * 0.005f;
        camPhi_ -= dx;
        camTheta_ = std::max(0.05f, std::min(3.09f, camTheta_ + dy));
    }
    lastX_ = x;
    lastY_ = y;
}

void Renderer::handleScroll(double dy) {
    if (!std::isfinite(dy)) return;
    camDist_ = std::max(0.3f, camDist_ - static_cast<float>(dy) * 0.2f);
}

void Renderer::resetCamera() {
    camTheta_ = 1.2f;
    camPhi_ = 0.7f;
    camDist_ = 3.2f;
    camTarget_ = glm::vec3(0.f);
}

void Renderer::setCamera(float theta, float phi, float dist) {
    camTheta_ = std::max(0.05f, std::min(3.09f, theta));
    camPhi_ = phi;
    camDist_ = std::max(0.3f, dist);
}

glm::vec3 Renderer::eye() const {
    return camTarget_ + camDist_ * glm::vec3(std::sin(camTheta_) * std::cos(camPhi_), std::cos(camTheta_),
                                             std::sin(camTheta_) * std::sin(camPhi_));
}

glm::mat4 Renderer::computeView() const { return glm::lookAt(eye(), camTarget_, glm::vec3(0.f, 1.f, 0.f)); }

glm::mat4 Renderer::computeProj() const {
    const float aspect = extent_.height > 0 ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height) : 1.0f;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 100.0f);
    proj[1][1] *= -1.f;  // Vulkan NDC Y flip
    return proj;
}

Phantom::MeshEdit::MeshPickView Renderer::pickView() const {
    Phantom::MeshEdit::MeshPickView v;
    v.model = glm::mat4(1.f);
    v.view = computeView();
    v.proj = computeProj();
    v.width = static_cast<float>(extent_.width);
    v.height = static_cast<float>(extent_.height);
    return v;
}

// ---- IVkSubRenderer ----

void Renderer::onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool, VkRenderPass renderPass,
                      uint32_t framesInFlight) {
    ctx_ = &ctx;
    pool_ = &pool;

    Phantom::VKG::VkTriangleRenderer::Config triCfg;
    triCfg.vertSpv = std::move(shaders_.triVert);
    triCfg.fragSpv = std::move(shaders_.triFrag);
    triCfg.cullBack = false;   // nothing is culled: an inside-out mesh must stay visible (see the orientation view)
    triRenderer_.emplace(std::move(triCfg));
    triRenderer_->create(ctx, pool, renderPass, framesInFlight);

    Phantom::VKG::VkLineRenderer::Config lineCfg;
    lineCfg.vertSpv = std::move(shaders_.lineVert);
    lineCfg.fragSpv = std::move(shaders_.lineFrag);
    lineRenderer_.emplace(std::move(lineCfg));
    lineRenderer_->create(ctx, pool, renderPass, framesInFlight);

    Phantom::VKG::VkPointRenderer::Config ptCfg;
    ptCfg.vertSpv = std::move(shaders_.pointVert);
    ptCfg.fragSpv = std::move(shaders_.pointFrag);
    pointRenderer_.emplace(std::move(ptCfg));
    pointRenderer_->create(ctx, pool, renderPass, framesInFlight);

    world_->setEye(eye());
    uploadBuffers(world_->draw());
}

void Renderer::onUpdate(uint32_t frameIndex) {
    world_->setEye(eye());
    MeshDraw& draw = world_->draw();
    if (draw.dirty) {
        vkDeviceWaitIdle(ctx_->getDevice());
        uploadBuffers(draw);
        draw.dirty = false;
    }
    const glm::mat4 mvp = computeProj() * computeView();
    triRenderer_->updateMVP(frameIndex, mvp);
    lineRenderer_->updateMVP(frameIndex, mvp);
    if (hasPoints_) {
        // The point renderer carries its matrices in the upload buffer, so refresh them with the camera.
        Phantom::VKG::VkPointRenderer::Buffer ptBuf;
        ptBuf.positions = draw.pointPositions;
        ptBuf.colors = draw.pointColors;
        ptBuf.sizes = draw.pointSizes;
        ptBuf.modelViewMatrix = glm::mat4(1.f);
        ptBuf.projectionMatrix = mvp;
        pointRenderer_->upload(*ctx_, *pool_, ptBuf);
    }
}

void Renderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (triRenderer_ && triRenderer_->isValid() && hasTriangles_) triRenderer_->render(cmd, frameIndex);
    if (lineRenderer_ && lineRenderer_->isValid() && hasLines_) lineRenderer_->render(cmd, frameIndex);
    if (pointRenderer_ && pointRenderer_->isValid() && hasPoints_) pointRenderer_->render(cmd, frameIndex);
}

void Renderer::onCleanup(VkDevice device) {
    if (triRenderer_) triRenderer_->destroy(device);
    if (lineRenderer_) lineRenderer_->destroy(device);
    if (pointRenderer_) pointRenderer_->destroy(device);
}

// ---- private ----

void Renderer::buildAxisGeometry() {
    axisPositions_.clear();
    axisColors_.clear();
    axisIndices_.clear();
    auto add = [this](const glm::vec3& a, const glm::vec3& b, const glm::vec4& c) {
        const uint32_t base = static_cast<uint32_t>(axisPositions_.size() / 3);
        for (const glm::vec3& p : { a, b }) { axisPositions_.push_back(p.x); axisPositions_.push_back(p.y); axisPositions_.push_back(p.z); }
        for (int i = 0; i < 2; ++i) { axisColors_.push_back(c.r); axisColors_.push_back(c.g); axisColors_.push_back(c.b); axisColors_.push_back(c.a); }
        axisIndices_.push_back(base);
        axisIndices_.push_back(base + 1);
    };
    add(glm::vec3(0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
    add(glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
    add(glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec4(0.2f, 0.4f, 1.0f, 1.0f));
}

void Renderer::uploadBuffers(MeshDraw& draw) {
    const glm::mat4 mvp = computeProj() * computeView();

    Phantom::VKG::VkTriangleRenderer::Buffer triBuf;
    triBuf.positions = draw.triPositions;
    triBuf.colors = draw.triColors;
    triBuf.indices = draw.triIndices;
    triBuf.modelViewMatrix = glm::mat4(1.f);
    triBuf.projectionMatrix = mvp;
    hasTriangles_ = !triBuf.indices.empty();
    if (hasTriangles_) triRenderer_->upload(*ctx_, *pool_, triBuf);

    Phantom::VKG::VkLineRenderer::Buffer lineBuf;
    lineBuf.positions = axisPositions_;
    lineBuf.positions.insert(lineBuf.positions.end(), draw.linePositions.begin(), draw.linePositions.end());
    lineBuf.colors = axisColors_;
    lineBuf.colors.insert(lineBuf.colors.end(), draw.lineColors.begin(), draw.lineColors.end());
    lineBuf.indices = axisIndices_;
    const uint32_t offset = static_cast<uint32_t>(axisPositions_.size() / 3);
    for (uint32_t i : draw.lineIndices) lineBuf.indices.push_back(offset + i);
    lineBuf.modelViewMatrix = glm::mat4(1.f);
    lineBuf.projectionMatrix = mvp;
    lineRenderer_->upload(*ctx_, *pool_, lineBuf);
    hasLines_ = !lineBuf.indices.empty();

    hasPoints_ = !draw.pointSizes.empty();
    if (hasPoints_) {
        Phantom::VKG::VkPointRenderer::Buffer ptBuf;
        ptBuf.positions = draw.pointPositions;
        ptBuf.colors = draw.pointColors;
        ptBuf.sizes = draw.pointSizes;
        ptBuf.modelViewMatrix = glm::mat4(1.f);
        ptBuf.projectionMatrix = mvp;
        pointRenderer_->upload(*ctx_, *pool_, ptBuf);
    }
}

}  // namespace MeshView
