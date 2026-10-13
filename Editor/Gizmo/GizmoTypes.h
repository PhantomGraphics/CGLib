#pragma once

#include <array>
#include "CGLib/Math/Vector2d.h"

namespace Phantom::Gizmo {

enum class GizmoMode { Translate, Rotate, Scale };

// Transform of the manipulated object
struct GizmoTransform {
    std::array<float, 3> translation      = {0.f, 0.f, 0.f};
    std::array<float, 3> rotationEulerDeg = {0.f, 0.f, 0.f};
    std::array<float, 3> scale            = {1.f, 1.f, 1.f};
};

struct GizmoMouseState {
    Phantom::Math::Vector2df pos          = {0.f, 0.f};
    bool      leftDown     = false;
    bool      leftPressed  = false;
    bool      leftReleased = false;
};

} // namespace Phantom::Gizmo
