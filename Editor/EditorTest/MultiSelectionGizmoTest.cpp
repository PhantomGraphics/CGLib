#include <gtest/gtest.h>
#include "../Gizmo/MultiSelectionGizmo.h"
#include <vector>

using namespace Phantom::Gizmo;
using namespace Phantom::Editor;

TEST(MultiSelectionGizmoTest, CentroidAndPivotDeltas) {
    std::vector<GizmoTransform> transforms(2);
    transforms[0].translation = {0.f, 0.f, 0.f};
    transforms[1].translation = {4.f, 2.f, -2.f};
    const auto c = selectionCentroid(transforms);
    EXPECT_EQ(c, (std::array<float, 3>{2.f, 1.f, -1.f}));
    EXPECT_EQ(selectionCentroid({}), (std::array<float, 3>{0.f, 0.f, 0.f}));

    GizmoTransform start = transforms[1];
    start.rotationEulerDeg = {10.f, 20.f, 30.f};
    start.scale = {2.f, 2.f, 2.f};
    GizmoTransform pivotStart;
    pivotStart.translation = c;
    GizmoTransform now = pivotStart;
    now.translation = {5.f, 1.f, -1.f};
    now.rotationEulerDeg = {0.f, 45.f, 0.f};
    now.scale = {3.f, 1.f, 1.f};
    // Translate mode changes only the translation, by the pivot's delta.
    auto moved = applyPivotDelta(GizmoMode::Translate, start, pivotStart, now);
    EXPECT_EQ(moved.translation, (std::array<float, 3>{7.f, 2.f, -2.f}));
    EXPECT_EQ(moved.rotationEulerDeg, start.rotationEulerDeg);
    EXPECT_EQ(moved.scale, start.scale);
    // Rotate mode adds the Euler delta.
    auto rotated = applyPivotDelta(GizmoMode::Rotate, start, pivotStart, now);
    EXPECT_EQ(rotated.rotationEulerDeg, (std::array<float, 3>{10.f, 65.f, 30.f}));
    EXPECT_EQ(rotated.translation, start.translation);
    // Scale mode multiplies by the pivot's per-axis ratio; a zero base leaves the axis alone.
    auto scaled = applyPivotDelta(GizmoMode::Scale, start, pivotStart, now);
    EXPECT_EQ(scaled.scale, (std::array<float, 3>{6.f, 2.f, 2.f}));
    GizmoTransform zero = pivotStart;
    zero.scale = {0.f, 1.f, 1.f};
    EXPECT_EQ(applyPivotDelta(GizmoMode::Scale, start, zero, now).scale[0], 2.f);
}
