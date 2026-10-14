#pragma once

#include "GizmoTypes.h"
#include <span>

namespace Phantom::Editor {

// Virtual selection pivot. Translation/Euler angles are additive; scale uses
// per-axis ratios. This does not rotate or scale object positions around the pivot.
inline std::array<float, 3> selectionCentroid(std::span<const Gizmo::GizmoTransform> transforms) {
    std::array<float, 3> sum = {0.f, 0.f, 0.f};
    for (const auto& t : transforms)
        for (int i = 0; i < 3; ++i) sum[i] += t.translation[i];
    if (!transforms.empty())
        for (int i = 0; i < 3; ++i) sum[i] /= static_cast<float>(transforms.size());
    return sum;
}

inline Gizmo::GizmoTransform applyPivotDelta(Gizmo::GizmoMode mode,
    const Gizmo::GizmoTransform& start, const Gizmo::GizmoTransform& pivotStart,
    const Gizmo::GizmoTransform& pivotNow)
{
    Gizmo::GizmoTransform result = start;
    for (int i = 0; i < 3; ++i) {
        if (mode == Gizmo::GizmoMode::Translate) {
            result.translation[i] = start.translation[i] + (pivotNow.translation[i] - pivotStart.translation[i]);
        } else if (mode == Gizmo::GizmoMode::Rotate) {
            result.rotationEulerDeg[i] = start.rotationEulerDeg[i] +
                (pivotNow.rotationEulerDeg[i] - pivotStart.rotationEulerDeg[i]);
        } else {
            const float base = pivotStart.scale[i];
            const float ratio = base != 0.f ? pivotNow.scale[i] / base : 1.f;
            result.scale[i] = start.scale[i] * ratio;
        }
    }
    return result;
}

} // namespace Phantom::Editor
