#include "Renderer.h"

#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace VKSpace {

// ============================================================
//  Construction
// ============================================================

Renderer::Renderer(World* world)
    : world_(world)
{
    buildAxisGeometry();
}

// ============================================================
//  Mouse / camera input
// ============================================================

void Renderer::handleMouseButton(bool leftPressed) {
    isDragging_ = leftPressed;
}

void Renderer::handleMouseMove(double x, double y) {
    if (isDragging_) {
        const float dx = static_cast<float>(x - lastX_) * 0.005f;
        const float dy = static_cast<float>(y - lastY_) * 0.005f;
        camPhi_   -= dx;
        camTheta_  = std::max(0.05f, std::min(3.09f, camTheta_ + dy));
    }
    lastX_ = x;
    lastY_ = y;
}

void Renderer::handleScroll(double dy) {
    camDist_ = std::max(0.1f, camDist_ - static_cast<float>(dy) * 0.2f);
}

void Renderer::resetCamera() {
    camTheta_  = 1.0f;
    camPhi_    = 0.5f;
    camDist_   = 4.0f;
    camTarget_ = glm::vec3(0.f, 0.f, 0.f);
}

// ============================================================
//  MVP
// ============================================================

glm::mat4 Renderer::computeMVP() const {
    const float aspect = (extent_.height > 0)
        ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
        : 1.0f;

    const glm::vec3 eye = camTarget_ + camDist_ * glm::vec3(
        std::sin(camTheta_) * std::cos(camPhi_),
        std::cos(camTheta_),
        std::sin(camTheta_) * std::sin(camPhi_));

    const glm::mat4 view = glm::lookAt(eye, camTarget_, glm::vec3(0.f, 1.f, 0.f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 100.0f);
    proj[1][1] *= -1.f; // Vulkan NDC Y-flip

    return proj * view;
}

// ============================================================
//  IVkSubRenderer
// ============================================================

void Renderer::onInit(Phantom::VKG::VulkanContext& ctx,
                              const Phantom::VKG::VulkanCommandPool& pool,
                              VkRenderPass renderPass,
                              uint32_t framesInFlight)
{
    ctx_  = &ctx;
    pool_ = &pool;

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

    uploadBuffers();
}

void Renderer::onUpdate(uint32_t frameIndex) {
    auto& res = world_->getResult();

    if (res.dirty) {
        vkDeviceWaitIdle(ctx_->getDevice());
        uploadBuffers();
        res.dirty = false;
    }

    const glm::mat4 mvp = computeMVP();
    lineRenderer_->updateMVP(frameIndex, mvp);

    if (!res.pointSizes.empty()) {
        Phantom::VKG::VkPointRenderer::Buffer ptBuf;
        ptBuf.positions        = res.pointPositions;
        ptBuf.colors           = res.pointColors;
        ptBuf.sizes            = res.pointSizes;
        ptBuf.modelViewMatrix  = glm::mat4(1.f);
        ptBuf.projectionMatrix = mvp;
        pointRenderer_->upload(*ctx_, *pool_, ptBuf);
    }
}

void Renderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (lineRenderer_ && lineRenderer_->isValid() && hasLineGeometry_)
        lineRenderer_->render(cmd, frameIndex);

    if (pointRenderer_ && pointRenderer_->isValid() && !world_->getResult().pointSizes.empty())
        pointRenderer_->render(cmd, frameIndex);
}

void Renderer::onCleanup(VkDevice device) {
    if (lineRenderer_)  lineRenderer_->destroy(device);
    if (pointRenderer_) pointRenderer_->destroy(device);
}

// ============================================================
//  Private
// ============================================================

void Renderer::buildAxisGeometry() {
    axisPositions_.clear();
    axisColors_.clear();
    axisIndices_.clear();

    auto addAxisLine = [this](const glm::vec3& a, const glm::vec3& b, const glm::vec4& c) {
        const uint32_t base = static_cast<uint32_t>(axisPositions_.size() / 3);

        axisPositions_.push_back(a.x);
        axisPositions_.push_back(a.y);
        axisPositions_.push_back(a.z);
        axisColors_.push_back(c.r);
        axisColors_.push_back(c.g);
        axisColors_.push_back(c.b);
        axisColors_.push_back(c.a);

        axisPositions_.push_back(b.x);
        axisPositions_.push_back(b.y);
        axisPositions_.push_back(b.z);
        axisColors_.push_back(c.r);
        axisColors_.push_back(c.g);
        axisColors_.push_back(c.b);
        axisColors_.push_back(c.a);

        axisIndices_.push_back(base + 0);
        axisIndices_.push_back(base + 1);
    };

    addAxisLine(glm::vec3(0.f, 0.f, 0.f), glm::vec3(1.f, 0.f, 0.f), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
    addAxisLine(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f), glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
    addAxisLine(glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f), glm::vec4(0.2f, 0.4f, 1.0f, 1.0f));
}

void Renderer::uploadBuffers() {
    const auto& res = world_->getResult();
    const glm::mat4 mvp = computeMVP();

    // Line renderer
    Phantom::VKG::VkLineRenderer::Buffer lineBuf;
    lineBuf.positions       = axisPositions_;
    lineBuf.positions.insert(lineBuf.positions.end(), res.linePositions.begin(), res.linePositions.end());
    lineBuf.colors          = axisColors_;
    lineBuf.colors.insert(lineBuf.colors.end(), res.lineColors.begin(), res.lineColors.end());
    lineBuf.indices         = axisIndices_;
    const uint32_t indexOffset = static_cast<uint32_t>(axisPositions_.size() / 3);
    lineBuf.indices.reserve(lineBuf.indices.size() + res.lineIndices.size());
    for (uint32_t i : res.lineIndices)
        lineBuf.indices.push_back(indexOffset + i);
    lineBuf.modelViewMatrix = glm::mat4(1.f);
    lineBuf.projectionMatrix = mvp;
    lineRenderer_->upload(*ctx_, *pool_, lineBuf);
    hasLineGeometry_ = !lineBuf.indices.empty();

    // Point renderer
    if (!res.pointSizes.empty()) {
        Phantom::VKG::VkPointRenderer::Buffer ptBuf;
        ptBuf.positions        = res.pointPositions;
        ptBuf.colors           = res.pointColors;
        ptBuf.sizes            = res.pointSizes;
        ptBuf.modelViewMatrix  = glm::mat4(1.f);
        ptBuf.projectionMatrix = mvp;
        pointRenderer_->upload(*ctx_, *pool_, ptBuf);
    }
}

} // namespace VKSpace
