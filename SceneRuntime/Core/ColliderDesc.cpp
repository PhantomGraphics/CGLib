#include "ColliderDesc.h"

#include <algorithm>

namespace Phantom::SceneRuntime {

ColliderDesc ColliderDesc::scaled(const glm::vec3& scale) const
{
    const glm::vec3 s = glm::abs(scale);
    ColliderDesc out = *this;
    out.center = center * scale;
    out.halfExtents = halfExtents * s;
    if (shape == "sphere") {
        out.radius = radius * std::max({ s.x, s.y, s.z });
    } else if (shape == "capsule") {
        const float totalHalf = (halfHeight + radius) * s.y;
        out.radius = radius * std::max(s.x, s.z);
        out.halfHeight = std::max(totalHalf - out.radius, 0.f);
    }
    if (points) {
        // Signed scale: a mirrored node mirrors its collider exactly like its render mesh.
        auto p = std::make_shared<std::vector<glm::vec3>>(*points);
        for (auto& v : *p) v *= scale;
        out.points = std::move(p);
    }
    return out;
}

bool isKnownColliderShape(const std::string& shape)
{
    return shape == "box" || shape == "sphere" || shape == "capsule" || shape == "plane"
        || shape == "convexHull" || shape == "mesh";
}

} // namespace Phantom::SceneRuntime
