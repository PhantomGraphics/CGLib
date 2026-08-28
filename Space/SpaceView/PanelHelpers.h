#pragma once

#include "World.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace VKSpace {

/// @brief Appends the 12 edges of an AABB as line segments into SpaceResult.
///
/// @param res    Target result buffer.
/// @param mn     Minimum corner of the box.
/// @param mx     Maximum corner of the box.
/// @param color  RGBA color of the box edges.
inline void addBoxWireframe(SpaceResult& res,
                            const glm::vec3& mn,
                            const glm::vec3& mx,
                            const glm::vec4& color = {1.f, 1.f, 1.f, 1.f})
{
    // 8 corners
    const glm::vec3 corners[8] = {
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
        {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
        {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
    };

    const uint32_t base = static_cast<uint32_t>(res.linePositions.size() / 3);

    for (const auto& c : corners) {
        res.linePositions.push_back(c.x);
        res.linePositions.push_back(c.y);
        res.linePositions.push_back(c.z);
        res.lineColors.push_back(color.r);
        res.lineColors.push_back(color.g);
        res.lineColors.push_back(color.b);
        res.lineColors.push_back(color.a);
    }

    // 12 edges of a box (index pairs)
    const uint32_t edges[24] = {
        0,1, 1,2, 2,3, 3,0,   // bottom face
        4,5, 5,6, 6,7, 7,4,   // top face
        0,4, 1,5, 2,6, 3,7    // vertical edges
    };
    for (uint32_t idx : edges)
        res.lineIndices.push_back(base + idx);
}

} // namespace VKSpace
