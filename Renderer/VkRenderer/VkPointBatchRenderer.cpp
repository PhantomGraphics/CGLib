#include "VkPointBatchRenderer.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <algorithm>
#include <cmath>

namespace Phantom::VKG {

namespace {

struct Vertex { float pos[3]; float color[3]; float radius; };
static_assert(sizeof(Vertex) == 28, "interleaved point vertex");

struct PushConstants {
    glm::mat4 mvp;
    glm::vec4 tint;
    glm::vec4 params;
};
static_assert(sizeof(PushConstants) <= 128, "push constant budget");

// Sphere mode (point_sphere.*): the whole push-constant budget, read by both stages.
struct SpherePushConstants {
    glm::mat4 mv;
    glm::vec4 tint;
    glm::vec4 proj;     // P[0][0], P[1][1], P[2][2], P[3][2]
    glm::vec4 viewport; // x, y, width, height
    glm::vec4 params;   // largest scale of mv, radius scale, min point size, srgb
};
static_assert(sizeof(SpherePushConstants) == 128, "push constant budget");

// Largest singular value of a 3x3 (the largest stretch it applies to a unit vector), by power iteration on A^T A.
float largestScale(const glm::mat3& a)
{
    const glm::mat3 m = glm::transpose(a) * a;
    glm::vec3 v(0.577f, 0.577f, 0.577f);
    float lambda = 0.f;
    for (int i = 0; i < 24; ++i) {
        const glm::vec3 w = m * v;
        const float n = std::sqrt(glm::dot(w, w));
        if (!(n > 1.0e-20f)) return 0.f;
        v = w / n;
        lambda = n;
    }
    return std::sqrt(lambda);
}

} // namespace

bool VkPointBatchRenderer::create(const VulkanContext& ctx, const VulkanCommandPool& pool, VkRenderPass renderPass)
{
    ctx_  = &ctx;
    pool_ = &pool;

    PipelineConfig p{};
    p.vertSpv = config_.vertSpv;
    p.fragSpv = config_.fragSpv;
    p.bindingDescs = { { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX } };
    p.attrDescs = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) },
        { 2, 0, VK_FORMAT_R32_SFLOAT,       offsetof(Vertex, radius) },
    };
    p.topology  = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    p.cullMode  = VK_CULL_MODE_NONE;
    p.depthTest = true;
    p.depthWrite = true;
    p.samples = config_.samples;
    p.pushConstantRanges = { { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants) } };
    if (!pipeline_.create(ctx, renderPass, p)) return false;

    if (!config_.sphereVertSpv.empty() && !config_.sphereFragSpv.empty()) {
        PipelineConfig s = p;
        s.vertSpv = config_.sphereVertSpv;
        s.fragSpv = config_.sphereFragSpv;
        s.pushConstantRanges = { { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SpherePushConstants) } };
        // A failed sphere pipeline only disables sphere draws; the sprite path above stays usable.
        spherePipeline_.create(ctx, renderPass, s);
    }
    return true;
}

void VkPointBatchRenderer::destroy(VkDevice device)
{
    if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
    for (auto& kv : batches_) kv.second.buffer.destroy(device);
    batches_.clear();
    pipeline_.destroy(device);
    spherePipeline_.destroy(device);
}

uint32_t VkPointBatchRenderer::addBatch(const PointBatchData& d)
{
    if (!ctx_ || !pool_ || !d.positions || d.count == 0) return 0;
    const uint64_t n = std::min<uint64_t>(d.count, 0xFFFFFFFFull);
    std::vector<Vertex> verts(static_cast<size_t>(n));
    for (uint64_t i = 0; i < n; ++i) {
        Vertex& v = verts[static_cast<size_t>(i)];
        for (int k = 0; k < 3; ++k) {
            v.pos[k]   = d.positions[i * 3 + k];
            v.color[k] = d.colors ? d.colors[i * 3 + k] : d.defaultColor[k];
        }
        v.radius = d.radii ? d.radii[i] : d.defaultRadius;
    }
    Batch b;
    if (!b.buffer.create(*ctx_, *pool_, verts.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, verts.data()))
        return 0;
    b.count = static_cast<uint32_t>(n);
    b.bytes = verts.size() * sizeof(Vertex);
    const uint32_t id = nextId_++;
    batches_.emplace(id, std::move(b));
    ++uploads_;
    return id;
}

void VkPointBatchRenderer::removeBatch(uint32_t id)
{
    auto it = batches_.find(id);
    if (it == batches_.end()) return;
    if (ctx_) vkDeviceWaitIdle(ctx_->getDevice());
    it->second.buffer.destroy(ctx_ ? ctx_->getDevice() : VK_NULL_HANDLE);
    batches_.erase(it);
}

uint64_t VkPointBatchRenderer::batchPointCount(uint32_t id) const
{
    auto it = batches_.find(id);
    return it == batches_.end() ? 0 : it->second.count;
}

uint64_t VkPointBatchRenderer::gpuBytes() const
{
    uint64_t sum = 0;
    for (const auto& kv : batches_) sum += kv.second.bytes;
    return sum;
}

void VkPointBatchRenderer::setCamera(const glm::mat4& view, const glm::mat4& proj, uint32_t viewportHeightPx)
{
    view_ = view;
    proj_ = proj;
    targetHeight_ = viewportHeightPx;
    viewProj_ = proj * view;
    pixelsPerUnit_ = 0.5f * static_cast<float>(viewportHeightPx) * std::abs(proj[1][1]);
}

void VkPointBatchRenderer::render(VkCommandBuffer cmd, const std::vector<PointBatchDraw>& draws) const
{
    if (!isValid() || draws.empty()) return;
    enum class Bound { None, Sprite, Sphere } bound = Bound::None;
    for (const auto& d : draws) {
        auto it = batches_.find(d.batch);
        if (it == batches_.end() || it->second.count == 0) continue;
        if (d.sphere && hasSpherePipeline()) {
            if (bound != Bound::Sphere) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline_.getPipeline());
                bound = Bound::Sphere;
            }
            SpherePushConstants sp;
            sp.mv = view_ * d.model;
            sp.tint = glm::vec4(d.tint, d.useTint ? 1.f : 0.f);
            sp.proj = glm::vec4(proj_[0][0], proj_[1][1], proj_[2][2], proj_[3][2]);
            sp.viewport = vpSet_ ? vp_ : glm::vec4(0.f, 0.f, float(targetHeight_) * std::abs(proj_[1][1]) / std::max(std::abs(proj_[0][0]), 1.0e-6f), float(targetHeight_));
            sp.params = glm::vec4(largestScale(glm::mat3(sp.mv)), d.radiusScale, minPointSize_, srgbOutput_ ? 1.f : 0.f);
            vkCmdPushConstants(cmd, spherePipeline_.getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(sp), &sp);
            VkBuffer     svb  = it->second.buffer.get();
            VkDeviceSize soff = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &svb, &soff);
            vkCmdDraw(cmd, it->second.count, 1, 0, 0);
            continue;
        }
        if (bound != Bound::Sprite) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());
            bound = Bound::Sprite;
        }
        PushConstants pc;
        pc.mvp = viewProj_ * d.model;
        pc.tint = glm::vec4(d.tint, d.useTint ? 1.f : 0.f);
        pc.params = glm::vec4(pixelsPerUnit_, d.radiusScale, minPointSize_, srgbOutput_ ? 1.f : 0.f);
        vkCmdPushConstants(cmd, pipeline_.getLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
        VkBuffer     vb  = it->second.buffer.get();
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
        vkCmdDraw(cmd, it->second.count, 1, 0, 0);
    }
}

} // namespace Phantom::VKG
