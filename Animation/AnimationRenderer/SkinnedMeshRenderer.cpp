#include "CGLib/Animation/AnimationRenderer/SkinnedMeshRenderer.h"

#include "CGLib/Animation/Core/SkinningCpu.h"
#include "CGLib/VulkanGraphics/VulkanContext.h"
#include "CGLib/VulkanGraphics/VulkanCommandPool.h"

namespace Phantom::Animation {

void SkinnedMeshRenderer::updatePose(const std::vector<glm::mat4>& skinMatrices,
                                      const glm::mat4& mvp)
{
    skinMatrices_ = skinMatrices;
    mvp_          = mvp;

    if (mesh_.vertices.empty()) {
        dirty_ = true;
        return;
    }

    const std::size_t n = mesh_.vertices.size();
    skinnedPositions_.assign(n * 3, 0.f);
    skinVertices(mesh_.vertices.data(), n, skinMatrices_.data(), skinMatrices_.size(), skinnedPositions_.data(), nullptr);
    skinnedColors_.clear();
    skinnedColors_.reserve(n * 4);
    for (const auto& v : mesh_.vertices) {
        skinnedColors_.push_back(v.color.r);
        skinnedColors_.push_back(v.color.g);
        skinnedColors_.push_back(v.color.b);
        skinnedColors_.push_back(v.color.a);
    }
    dirty_ = true;
}

void SkinnedMeshRenderer::onInit(::Phantom::VKG::VulkanContext& ctx,
                                  const ::Phantom::VKG::VulkanCommandPool& pool,
                                  VkRenderPass renderPass,
                                  uint32_t framesInFlight)
{
    ctx_  = &ctx;
    pool_ = &pool;

    Phantom::VKG::VkTriangleRenderer::Config cfg;
    cfg.vertSpv   = std::move(shaders_.vertSpv);
    cfg.fragSpv   = std::move(shaders_.fragSpv);
    cfg.cullBack  = false; // show both sides of the bone diamonds
    triRenderer_.emplace(std::move(cfg));
    triRenderer_->create(ctx, pool, renderPass, framesInFlight);
}

void SkinnedMeshRenderer::onUpdate(uint32_t frameIndex)
{
    if (!triRenderer_) return;

    if (dirty_ && !skinnedPositions_.empty()) {
        vkDeviceWaitIdle(ctx_->getDevice());

        Phantom::VKG::VkTriangleRenderer::Buffer buf;
        buf.positions        = skinnedPositions_;
        buf.colors           = skinnedColors_;
        buf.indices          = mesh_.indices;
        buf.projectionMatrix = mvp_;
        buf.modelViewMatrix  = glm::mat4{1.f};
        triRenderer_->upload(*ctx_, *pool_, buf);
        dirty_ = false;
    }

    triRenderer_->updateMVP(frameIndex, mvp_);
}

void SkinnedMeshRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!visible_ || !triRenderer_ || !triRenderer_->isValid()) return;
    if (mesh_.indices.empty()) return;
    triRenderer_->render(cmd, frameIndex);
}

void SkinnedMeshRenderer::onCleanup(VkDevice device)
{
    if (triRenderer_) triRenderer_->destroy(device);
}

} // namespace Phantom::Animation
