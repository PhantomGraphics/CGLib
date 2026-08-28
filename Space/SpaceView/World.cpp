#include "World.h"

namespace VKSpace {

void SpaceResult::addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color) {
    const uint32_t base = static_cast<uint32_t>(linePositions.size() / 3);

    linePositions.push_back(a.x);
    linePositions.push_back(a.y);
    linePositions.push_back(a.z);
    lineColors.push_back(color.r);
    lineColors.push_back(color.g);
    lineColors.push_back(color.b);
    lineColors.push_back(color.a);

    linePositions.push_back(b.x);
    linePositions.push_back(b.y);
    linePositions.push_back(b.z);
    lineColors.push_back(color.r);
    lineColors.push_back(color.g);
    lineColors.push_back(color.b);
    lineColors.push_back(color.a);

    lineIndices.push_back(base + 0);
    lineIndices.push_back(base + 1);
}

void SpaceResult::addBoxWireframe(const glm::vec3& mn,
                                  const glm::vec3& mx,
                                  const glm::vec4& color) {
    const glm::vec3 corners[8] = {
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
        {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
        {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
    };

    const uint32_t base = static_cast<uint32_t>(linePositions.size() / 3);

    for (const auto& c : corners) {
        linePositions.push_back(c.x);
        linePositions.push_back(c.y);
        linePositions.push_back(c.z);
        lineColors.push_back(color.r);
        lineColors.push_back(color.g);
        lineColors.push_back(color.b);
        lineColors.push_back(color.a);
    }

    const uint32_t edges[24] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    for (uint32_t idx : edges)
        lineIndices.push_back(base + idx);
}

void SpaceResult::addPoint(const glm::vec3& pos, const glm::vec4& color, float size) {
    pointPositions.push_back(pos.x);
    pointPositions.push_back(pos.y);
    pointPositions.push_back(pos.z);

    pointColors.push_back(color.r);
    pointColors.push_back(color.g);
    pointColors.push_back(color.b);
    pointColors.push_back(color.a);

    pointSizes.push_back(size);
}

void SpaceResult::clear() {
    linePositions.clear();
    lineColors.clear();
    lineIndices.clear();
    pointPositions.clear();
    pointColors.clear();
    pointSizes.clear();
    dirty = false;
}

} // namespace VKSpace
