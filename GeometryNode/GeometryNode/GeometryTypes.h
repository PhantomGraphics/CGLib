#pragma once

// Phantom::GeometryNode -- geometry data model.
//
// CPU-only, no dependency on Math/GLM/Vulkan/WPF or any Studio type. Conventions
// (the contract an adapter converts from/to):
//   * Right-handed coordinates, +Y up, unit = 1 scene unit (metre).
//   * Triangles are counter-clockwise when seen from the outside (front face);
//     the face normal is (b - a) x (c - a). Any Y flip a renderer needs belongs
//     in the adapter, never in this module.
//   * normals / uvs are optional per-vertex attributes: either empty or exactly
//     positions.size() long. (Per-vertex only in the MVP -- no seam-aware UVs;
//     face/corner domains arrive with the attribute phase and are never
//     converted silently.)

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Phantom::GeometryNode {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
};

struct Bounds {
    bool valid = false;  // false for an empty mesh
    Vec3 min;
    Vec3 max;
};

struct Mesh;

// A reference to another geometry placed with its own T * R * S (rotation: Euler XYZ degrees, as
// TransformGeometry). Kept as a reference until Realize Instances expands it.
struct Instance {
    std::shared_ptr<const Mesh> source;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct Mesh {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;      // empty or positions.size()
    std::vector<Vec2> uvs;          // empty or positions.size()
    std::vector<uint32_t> indices;  // triangle list, size % 3 == 0
    // Points are a Mesh without indices. Instances (Phase 5) are carried unrealized; only Realize
    // Instances consumes them -- every other node (and Output) rejects a mesh that has some.
    std::vector<Instance> instances;

    size_t vertexCount() const { return positions.size(); }
    size_t triangleCount() const { return indices.size() / 3; }
};

// Evaluated geometry is immutable and shared between cache, downstream nodes
// and consumers. Never null in a successful result (an empty Mesh is valid).
using GeometryPtr = std::shared_ptr<const Mesh>;

// Resource ceilings, checked before any allocation.
struct Limits {
    uint64_t maxVertices = 8ull * 1024 * 1024;
    uint64_t maxIndices = 24ull * 1024 * 1024;
    uint64_t maxInstances = 1ull * 1024 * 1024;  // checked when instancing / realizing
    uint64_t maxMemoryBytes = 1024ull * 1024 * 1024;
};

}  // namespace Phantom::GeometryNode
