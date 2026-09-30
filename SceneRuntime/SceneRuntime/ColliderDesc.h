#pragma once

#include "CGLib/Math/Vector3d.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Phantom::SceneRuntime {

// A collider as authored (Blender phantom.asset/1 sidecar "collider" block, or a .universe
// rigidBody block): shape name plus the sizes that shape uses, in the entity's local frame.
// `center` is the collider center relative to the entity origin -- Blender objects often have
// their origin away from the geometry center (e.g. at the base of a pillar).
//
// Moved here from CGApp/Universe/Entity/RigidBodyComponent.h so Universe and PhantomStudio share
// one definition (docs/todo/PLAN_blender_universe_authoring_loop.md stage 1). Pure data + math;
// Universe re-exports it as Universe::ColliderDesc.
struct ColliderDesc {
    std::string shape = "box";            // box | sphere | capsule | plane | convexHull | mesh
    glm::vec3   halfExtents = { 0.5f, 0.5f, 0.5f };
    float       radius = 0.5f;
    float       halfHeight = 0.5f;        // capsule: half the straight part, along local Y
    glm::vec3   center = { 0.f, 0.f, 0.f };
    // Plane only: use this world-space plane as-is instead of deriving it from the entity
    // transform (saved .universe planes, and legacy files whose plane is always y = 0).
    bool        explicitPlane = false;
    glm::vec3   planeNormal = { 0.f, 1.f, 0.f };
    float       planeOffset = 0.f;
    // convexHull / mesh: the glTF node's own geometry (never stored in the sidecar or the
    // .universe -- heavy data stays in the GLB). `points` are in the entity's local frame;
    // `indices` (mesh only) is a triangle list. Shared and immutable, so copying a
    // ColliderDesc stays cheap. `sourceNode`/`sourceWorld` say where it came from so
    // LoadScene can read it again.
    std::shared_ptr<const std::vector<glm::vec3>> points;
    std::shared_ptr<const std::vector<uint32_t>>  indices;
    std::string sourceNode;
    bool        sourceWorld = false;

    bool needsGeometry() const { return shape == "convexHull" || shape == "mesh"; }

    // The same collider under a (possibly non-uniform) entity scale: box extents and the
    // center scale per axis, a sphere takes the largest axis, a capsule keeps its total
    // length along Y and takes the larger of X/Z for its radius; hull / mesh points scale
    // per axis exactly. Result is in world units but still oriented in the entity's local frame.
    ColliderDesc scaled(const glm::vec3& scale) const;
};

bool isKnownColliderShape(const std::string& shape);

} // namespace Phantom::SceneRuntime
