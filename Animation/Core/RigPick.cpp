#include "RigPick.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Phantom::Animation::RigPick {

std::vector<glm::vec3> sourceOf(const RigMesh& mesh, const std::vector<glm::vec3>& perRender)
{
    std::vector<glm::vec3> out(mesh.sourceCount, glm::vec3(0.f));
    std::vector<uint8_t> set(mesh.sourceCount, 0);
    for (size_t i = 0; i < perRender.size() && i < mesh.sourceIndex.size(); ++i) {
        const uint32_t s = mesh.sourceIndex[i];
        if (s < out.size() && !set[s]) { out[s] = perRender[i]; set[s] = 1; }
    }
    return out;
}

WeightView buildWeightView(const RigMesh& mesh, const std::vector<glm::vec3>& posedRender, const glm::mat4& model, const glm::mat4& view,
                           const glm::mat4& proj, float width, float height, bool through)
{
    WeightView v;
    const std::vector<glm::vec3> posed = sourceOf(mesh, posedRender);
    v.pixel.assign(posed.size(), glm::vec2(0.f));
    v.depth.assign(posed.size(), 0.f);
    v.onScreen.assign(posed.size(), 0);
    v.visible.assign(posed.size(), 0);
    const glm::mat4 mvp = proj * view * model;
    auto project = [&](const glm::vec3& p, glm::vec2& px, float& w) {
        const glm::vec4 c = mvp * glm::vec4(p, 1.f);
        w = c.w;
        if (!(c.w > 1e-6f)) return false;
        px = { (c.x / c.w * 0.5f + 0.5f) * width, (c.y / c.w * 0.5f + 0.5f) * height };
        return true;
    };
    for (size_t i = 0; i < posed.size(); ++i) {
        glm::vec2 px(0.f);
        float w = 0.f;
        const bool front = project(posed[i], px, w);
        v.pixel[i] = px;
        v.depth[i] = w;
        v.onScreen[i] = front && px.x >= 0.f && px.y >= 0.f && px.x < width && px.y < height;
    }
    if (through || posedRender.size() != mesh.vertices.size()) { v.visible = v.onScreen; return v; }

    // Coarse depth buffer of the posed triangles (perspective-correct depth = 1 / interpolated (1/w)).
    const float scale = 320.f / std::max(width, height);
    const int bw = std::max(1, static_cast<int>(width * scale)), bh = std::max(1, static_cast<int>(height * scale));
    std::vector<float> buffer(static_cast<size_t>(bw) * bh, std::numeric_limits<float>::infinity());
    struct P { float x, y, w; };
    auto toBuffer = [&](uint32_t index, P& out) {
        glm::vec2 px(0.f);
        float w = 0.f;
        if (!project(posedRender[index], px, w)) return false;
        out = { px.x * scale, px.y * scale, w };
        return true;
    };
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        P a, b, c;
        if (!toBuffer(mesh.indices[t], a) || !toBuffer(mesh.indices[t + 1], b) || !toBuffer(mesh.indices[t + 2], c)) continue;
        const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (std::fabs(area) < 1e-6f) continue;
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min({ a.x, b.x, c.x })))), x1 = std::min(bw - 1, static_cast<int>(std::ceil(std::max({ a.x, b.x, c.x }))));
        const int y0 = std::max(0, static_cast<int>(std::floor(std::min({ a.y, b.y, c.y })))), y1 = std::min(bh - 1, static_cast<int>(std::ceil(std::max({ a.y, b.y, c.y }))));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float px = x + 0.5f, py = y + 0.5f;
                const float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) / area;
                const float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) / area;
                const float w2 = 1.f - w0 - w1;
                if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f) continue;
                const float invW = w0 / a.w + w1 / b.w + w2 / c.w;
                if (!(invW > 0.f)) continue;
                float& d = buffer[static_cast<size_t>(y) * bw + x];
                d = std::min(d, 1.f / invW);
            }
    }
    for (size_t i = 0; i < posed.size(); ++i) {
        if (!v.onScreen[i]) continue;
        const int cx = static_cast<int>(v.pixel[i].x * scale), cy = static_cast<int>(v.pixel[i].y * scale);
        float nearest = std::numeric_limits<float>::infinity();
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const int x = std::clamp(cx + dx, 0, bw - 1), y = std::clamp(cy + dy, 0, bh - 1);
                nearest = std::min(nearest, buffer[static_cast<size_t>(y) * bw + x]);
            }
        v.visible[i] = !(nearest < std::numeric_limits<float>::infinity()) || v.depth[i] <= nearest * 1.03f + 0.02f;
    }
    return v;
}

int nearestVisibleVertex(const WeightView& view, float x, float y, float maxPx)
{
    int best = -1;
    float bestDistance = maxPx + 1.f;
    for (size_t i = 0; i < view.visible.size(); ++i) {
        if (!view.visible[i]) continue;
        const float d = glm::distance(view.pixel[i], glm::vec2(x, y));
        if (d > maxPx) continue;
        if (best < 0 || d < bestDistance - 0.5f || (d <= bestDistance + 0.5f && view.depth[i] < view.depth[best])) { best = static_cast<int>(i); bestDistance = d; }
    }
    return best;
}

std::vector<uint32_t> visibleInRectangle(const WeightView& view, float x0, float y0, float x1, float y1)
{
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    std::vector<uint32_t> out;
    for (size_t i = 0; i < view.visible.size(); ++i)
        if (view.visible[i] && view.pixel[i].x >= x0 && view.pixel[i].x <= x1 && view.pixel[i].y >= y0 && view.pixel[i].y <= y1)
            out.push_back(static_cast<uint32_t>(i));
    return out;
}

float autoBrushRadius(const RigMesh& mesh)
{
    glm::vec3 lo(1e30f), hi(-1e30f);
    for (const auto& v : mesh.vertices) { lo = glm::min(lo, v.position); hi = glm::max(hi, v.position); }
    const float diagonal = glm::distance(lo, hi);
    return diagonal > 0.f ? 0.08f * diagonal : 0.1f;
}

RigResult brushStroke(RigBinding& binding, const AuthoredRig& rig, const RigMesh& mesh, const WeightView& view, BrushMode mode,
                      uint32_t boneId, float strength, float radius, bool through, const std::vector<glm::vec2>& pixels, int& affected)
{
    affected = 0;
    if (view.visible.size() != mesh.sourceCount) return RigResult::failure("the view does not match the mesh");
    if (!(radius > 0.f)) radius = autoBrushRadius(mesh);
    const std::vector<glm::vec3> rest = RigSkinning::sourcePositions(mesh);
    const RigAdjacency adjacency = RigSkinning::buildAdjacency(mesh);
    for (const glm::vec2& px : pixels) {
        const int center = nearestVisibleVertex(view, px.x, px.y);
        if (center < 0) continue;
        const auto samples = RigSkinning::selectInRadius(rest, rest[center], radius, through ? nullptr : &view.visible);
        const RigResult r = RigSkinning::applyBrush(binding, rig, adjacency, mode, boneId, strength, samples);
        if (!r) return r;
        affected += static_cast<int>(samples.size());
    }
    return RigResult::success();
}

} // namespace Phantom::Animation::RigPick
