#include "TransferFunction.h"

#include <algorithm>
#include <cmath>

namespace Phantom::Volume {
namespace {

float clamp01(const float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

TFSample lerp(const TFSample& a,
              const TFSample& b,
              const float t) {
    TFSample out{};
    out.r = a.r + (b.r - a.r) * t;
    out.g = a.g + (b.g - a.g) * t;
    out.b = a.b + (b.b - a.b) * t;
    out.a = a.a + (b.a - a.a) * t;
    return out;
}

} // namespace

void TransferFunction::setPoint(const float scalar, const float r, const float g, const float b, const float a) {
    const float s = clamp01(scalar);

    auto it = std::find_if(points_.begin(), points_.end(), [s](const ControlPoint& p) {
        return std::fabs(p.s - s) < 1.0e-6f;
    });

    if (it != points_.end()) {
        it->r = clamp01(r);
        it->g = clamp01(g);
        it->b = clamp01(b);
        it->a = clamp01(a);
    } else {
        points_.push_back(ControlPoint{s, clamp01(r), clamp01(g), clamp01(b), clamp01(a)});
    }

    std::sort(points_.begin(), points_.end(), [](const ControlPoint& lhs, const ControlPoint& rhs) {
        return lhs.s < rhs.s;
    });
}

TFSample TransferFunction::sample(float scalar) const {
    if (points_.empty()) {
        return {};
    }

    scalar = clamp01(scalar);

    if (scalar <= points_.front().s) {
        return {points_.front().r, points_.front().g, points_.front().b, points_.front().a};
    }

    if (scalar >= points_.back().s) {
        return {points_.back().r, points_.back().g, points_.back().b, points_.back().a};
    }

    for (size_t i = 1; i < points_.size(); ++i) {
        const ControlPoint& left = points_[i - 1];
        const ControlPoint& right = points_[i];
        if (scalar <= right.s) {
            const float span = std::max(right.s - left.s, 1.0e-6f);
            const float t = (scalar - left.s) / span;
            return lerp(
                TFSample{left.r, left.g, left.b, left.a},
                TFSample{right.r, right.g, right.b, right.a},
                t);
        }
    }

    return {points_.back().r, points_.back().g, points_.back().b, points_.back().a};
}

void TransferFunction::buildLUT(int resolution) {
    if (resolution < 2) {
        resolution = 2;
    }

    lut_.resize(static_cast<size_t>(resolution));
    for (int i = 0; i < resolution; ++i) {
        const float s = static_cast<float>(i) / static_cast<float>(resolution - 1);
        lut_[static_cast<size_t>(i)] = sample(s);
    }
}

} // namespace PBVR
