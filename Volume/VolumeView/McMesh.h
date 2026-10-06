#pragma once

#include "World.h"

#include "../../Math/Triangle3d.h"

#include <array>
#include <string>
#include <vector>

namespace VolumeView {

// Flat-shaded triangle soup (3 unique vertices per triangle, sequential indices) with one RGBA
// colour for every vertex. Shared by the sparse and dense marching-cubes commands.
PolygonMesh trianglesToPolygonMesh(std::string name,
                                   const std::vector<Phantom::Math::Triangle3df>& tris,
                                   const std::array<float, 4>& rgba);

} // namespace VolumeView
