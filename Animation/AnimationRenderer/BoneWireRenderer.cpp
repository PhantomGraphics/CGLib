#include "CGLib/Animation/AnimationRenderer/BoneWireRenderer.h"

#include "CGLib/VulkanGraphics/VulkanContext.h"
#include "CGLib/VulkanGraphics/VulkanCommandPool.h"

namespace Phantom::Animation {

namespace {

void addLine(std::vector<float>& pos, std::vector<float>& col,
             std::vector<uint32_t>& idx,
             const glm::vec3& a, const glm::vec3& b, const glm::vec4& c)
{
    const uint32_t base = static_cast<uint32_t>(pos.size() / 3);
    pos.push_back(a.x); pos.push_back(a.y); pos.push_back(a.z);
    col.push_back(c.r); col.push_back(c.g); col.push_back(c.b); col.push_back(c.a);
    pos.push_back(b.x); pos.push_back(b.y); pos.push_back(b.z);
    col.push_back(c.r); col.push_back(c.g); col.push_back(c.b); col.push_back(c.a);
    idx.push_back(base + 0);
    idx.push_back(base + 1);
}

} // namespace

void BoneWireRenderer::updatePose(const Skeleton& skeleton,
                                   const std::vector<glm::mat4>& globalTransforms,
                                   const glm::mat4& mvp)
{
    mvp_ = mvp;
    positions_.clear();
    colors_.clear();
    indices_.clear();

    const int n = static_cast<int>(skeleton.bones.size());
    if (n == 0 || n != static_cast<int>(globalTransforms.size())) {
        dirty_ = true;
        return;
    }

    // Axis lines at origin
    addLine(positions_, colors_, indices_,
            {0.f,0.f,0.f}, {0.5f,0.f,0.f}, {1.f,0.3f,0.3f,1.f});
    addLine(positions_, colors_, indices_,
            {0.f,0.f,0.f}, {0.f,0.5f,0.f}, {0.3f,1.f,0.3f,1.f});
    addLine(positions_, colors_, indices_,
            {0.f,0.f,0.f}, {0.f,0.f,0.5f}, {0.3f,0.5f,1.f,1.f});

    // Bone lines: parent joint → child joint
    const glm::vec4 boneColor{0.9f, 0.8f, 0.2f, 1.f};
    for (int i = 0; i < n; ++i) {
        const Bone& bone = skeleton.bones[i];
        if (bone.parentIndex < 0) continue;

        glm::vec3 child  = glm::vec3(globalTransforms[i][3]);
        glm::vec3 parent = glm::vec3(globalTransforms[bone.parentIndex][3]);

        addLine(positions_, colors_, indices_, parent, child, boneColor);
    }

    dirty_ = true;
}

void BoneWireRenderer::onInit(::VKG::VulkanContext& ctx,
                               const ::VKG::VulkanCommandPool& pool,
                               VkRenderPass renderPass,
                               uint32_t framesInFlight)
{
    ctx_  = &ctx;
    pool_ = &pool;

    VKG::VkLineRenderer::Config cfg;
    cfg.vertSpv = std::move(shaders_.vertSpv);
    cfg.fragSpv = std::move(shaders_.fragSpv);
    lineRenderer_.emplace(std::move(cfg));
    lineRenderer_->create(ctx, pool, renderPass, framesInFlight);
}

void BoneWireRenderer::onUpdate(uint32_t frameIndex)
{
    if (!lineRenderer_ || !dirty_) {
        if (lineRenderer_) lineRenderer_->updateMVP(frameIndex, mvp_);
        return;
    }

    vkDeviceWaitIdle(ctx_->getDevice());

    VKG::VkLineRenderer::Buffer buf;
    buf.positions        = positions_;
    buf.colors           = colors_;
    buf.indices          = indices_;
    buf.projectionMatrix = mvp_;
    buf.modelViewMatrix  = glm::mat4{1.f};
    lineRenderer_->upload(*ctx_, *pool_, buf);
    lineRenderer_->updateMVP(frameIndex, mvp_);

    dirty_ = false;
}

void BoneWireRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!visible_ || !lineRenderer_ || !lineRenderer_->isValid()) return;
    if (indices_.empty()) return;
    lineRenderer_->render(cmd, frameIndex);
}

void BoneWireRenderer::onCleanup(VkDevice device)
{
    if (lineRenderer_) lineRenderer_->destroy(device);
}

} // namespace Phantom::Animation
