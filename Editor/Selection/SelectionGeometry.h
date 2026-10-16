#pragma once

#include "CGLib/Math/Core/Matrix4d.h"
#include "CGLib/Math/Core/Vector3d.h"
#include <cstdint>
#include <span>

namespace Phantom::Editor {

struct SelectionRectangle {
    float width, height;
    float x0, y0, x1, y1;
};

// Left-to-right: every vertex must be inside the viewport and rectangle.
// Right-to-left: any clipped triangle may touch the rectangle.
// MVP maps local positions into Vulkan clip space: -w<=x,y<=w, 0<=z<=w.
// NDC (-1,-1) maps to screen (0,0), matching the host's Vulkan projection.
// Invalid triangle indices are skipped; incomplete trailing triangles are ignored.
bool meshSelectedInRectangle(std::span<const Math::Vector3df> positions,
    std::span<const uint32_t> indices, const Math::Matrix4df& mvp,
    const SelectionRectangle& rectangle);

} // namespace Phantom::Editor
