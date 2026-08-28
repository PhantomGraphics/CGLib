#include "CGLib/Animation/AnimationRenderer/SkinnedMeshRenderer.h"

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

    const int maxBone = static_cast<int>(skinMatrices_.size());
    skinnedPositions_.clear();
    skinnedColors_.clear();
    skinnedPositions_.reserve(mesh_.vertices.size() * 3);
    skinnedColors_.reserve(mesh_.vertices.size() * 4);

    for (const auto& v : mesh_.vertices) {
        glm::vec4 p{v.position, 1.f};
        glm::vec4 sp{0.f};
        for (int k = 0; k < 4; ++k) {
            float w = v.boneWeights[k];
            if (w <= 0.f) continue;
            int bi = v.boneIndices[k];
            if (bi < 0 || bi >= maxBone) continue;
            sp += w * (skinMatrices_[bi] * p);
        }
        skinnedPositions_.push_back(sp.x);
        skinnedPositions_.push_back(sp.y);
        skinnedPositions_.push_back(sp.z);
        skinnedColors_.push_back(v.color.r);
        skinnedColors_.push_back(v.color.g);
        skinnedColors_.push_back(v.color.b);
        skinnedColors_.push_back(v.color.a);
    }

    dirty_ = true;
}

void SkinnedMeshRenderer::onInit(::VKG::VulkanContext& ctx,
                                  const ::VKG::VulkanCommandPool& pool,
                                  VkRenderPass renderPass,
                                  uint32_t framesInFlight)
{
    ctx_  = &ctx;
    pool_ = &pool;

    VKG::VkTriangleRenderer::Config cfg;
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

        VKG::VkTriangleRenderer::Buffer buf;
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
