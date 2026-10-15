#include "SelectionGeometry.h"

#include "CGLib/Math/Math/Vector2d.h"
#include "CGLib/Math/Math/Vector4d.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace Phantom::Editor {

bool meshSelectedInRectangle(std::span<const Math::Vector3df> positions,
    std::span<const uint32_t> indices, const Math::Matrix4df& mvp,
    const SelectionRectangle& rectangle)
{
    const auto [width, height, x0, y0, x1, y1] = rectangle;
    if (width <= 0.f || height <= 0.f || positions.empty() || indices.empty()) return false;
    const bool crossing = x1 < x0;
    const float left = std::min(x0, x1), right = std::max(x0, x1);
    const float top = std::min(y0, y1), bottom = std::max(y0, y1);
    if (left == right || top == bottom) return false;

    auto plane = [](const Math::Vector4df& p, int axis) -> float {
        switch (axis) {
        case 0: return p.x + p.w;
        case 1: return p.w - p.x;
        case 2: return p.y + p.w;
        case 3: return p.w - p.y;
        case 4: return p.z;
        default: return p.w - p.z;
        }
    };
    auto clipPlane = [&](std::vector<Math::Vector4df> poly, int axis) {
        std::vector<Math::Vector4df> out;
        if (poly.empty()) return out;
        Math::Vector4df prev = poly.back();
        float prevD = plane(prev, axis);
        for (const auto& cur : poly) {
            const float curD = plane(cur, axis);
            if ((prevD >= 0.f) != (curD >= 0.f)) {
                const float t = prevD / (prevD - curD);
                out.push_back(prev + t * (cur - prev));
            }
            if (curD >= 0.f) out.push_back(cur);
            prev = cur;
            prevD = curD;
        }
        return out;
    };
    auto screen = [&](const Math::Vector4df& p) {
        return Math::Vector2df((p.x / p.w + 1.f) * width * 0.5f,
                         (p.y / p.w + 1.f) * height * 0.5f);
    };
    auto screenPlane = [&](const Math::Vector2df& p, int axis) {
        switch (axis) {
        case 0: return p.x - left;
        case 1: return right - p.x;
        case 2: return p.y - top;
        default: return bottom - p.y;
        }
    };
    auto intersectsRect = [&](std::vector<Math::Vector2df> poly) {
        for (int axis = 0; axis < 4 && !poly.empty(); ++axis) {
            std::vector<Math::Vector2df> out;
            Math::Vector2df prev = poly.back();
            float prevD = screenPlane(prev, axis);
            for (const auto& cur : poly) {
                const float curD = screenPlane(cur, axis);
                if ((prevD >= 0.f) != (curD >= 0.f)) {
                    const float t = prevD / (prevD - curD);
                    out.push_back(prev + t * (cur - prev));
                }
                if (curD >= 0.f) out.push_back(cur);
                prev = cur;
                prevD = curD;
            }
            poly = std::move(out);
        }
        return !poly.empty();
    };

    std::vector<Math::Vector4df> points;
    points.reserve(positions.size());
    for (const auto& v : positions)
        points.push_back(mvp * Math::Vector4df(v[0], v[1], v[2], 1.f));

    if (!crossing) {
        bool contained = true;
        for (const auto& p : points) {
            if (p.w <= 0.f || p.z < 0.f || p.z > p.w ||
                p.x < -p.w || p.x > p.w || p.y < -p.w || p.y > p.w) {
                contained = false;
                break;
            }
            const auto xy = screen(p);
            if (xy.x < left || xy.x > right || xy.y < top || xy.y > bottom) {
                contained = false;
                break;
            }
        }
        return contained;
    }

    bool hit = false;
    for (size_t i = 0; i + 2 < indices.size() && !hit; i += 3) {
        const uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
        if (a >= points.size() || b >= points.size() || c >= points.size()) continue;
        std::vector<Math::Vector4df> poly{points[a], points[b], points[c]};
        for (int axis = 0; axis < 6 && !poly.empty(); ++axis)
            poly = clipPlane(std::move(poly), axis);
        if (poly.empty()) continue;
        std::vector<Math::Vector2df> projected;
        projected.reserve(poly.size());
        for (const auto& p : poly) {
            if (p.w > 0.f) projected.push_back(screen(p));
        }
        hit = intersectsRect(std::move(projected));
    }
    return hit;

}

} // namespace Phantom::Editor
