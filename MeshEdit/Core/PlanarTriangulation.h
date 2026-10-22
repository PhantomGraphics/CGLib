#pragma once
#include "CGLib/Math/Core/Vector3d.h"
#include <array>
#include <cstddef>
#include <vector>

namespace Phantom::MeshEdit::detail {
// A simple planar boundary, including collinear boundary vertices. No Steiner
// vertices are inserted. Output is committed only on success. Shared by Boolean
// reconstruction and planar cleanup; no Boolean or scene types are involved.
bool triangulatePlanarBoundary(const std::vector<Math::Vector3dd>& points,
                              const Math::Vector3dd& normal, double epsilon,
                              size_t& remainingWork, std::vector<std::array<size_t, 3>>& triangles);
}
