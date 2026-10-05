#pragma once

// Pure geometry operations used by the built-in nodes. Exception-free: every
// failure is an OpStatus; outputs are replaced only on success.

#include <atomic>
#include <string>

#include "GeometryTypes.h"

namespace Phantom::GeometryNode {

enum class OpStatus {
    Ok,
    InvalidArgument,   // non-finite or out-of-range parameter
    LimitExceeded,     // would exceed Limits (checked before allocating)
    SingularTransform, // determinant ~ 0 / zero scale
    InvalidMesh,       // inconsistent attributes, bad index, NaN/Inf
};

const char* toString(OpStatus s);

// Estimated heap footprint of a mesh with the given counts (overflow-safe,
// saturates at UINT64_MAX).
uint64_t estimateMeshBytes(uint64_t vertices, uint64_t indices);

// True if a mesh with these counts fits in `limits`.
bool fitsLimits(uint64_t vertices, uint64_t indices, const Limits& limits);

// Checks attribute sizes, index range / multiple of 3, and finiteness.
OpStatus validateMesh(const Mesh& mesh, std::string* message = nullptr);

Bounds computeBounds(const Mesh& mesh);

// Axis-aligned box centred on the origin, 24 vertices (flat normals, per-face UV).
OpStatus makeBox(const Vec3& size, const Limits& limits, Mesh& out);

// Grid on the XZ plane, +Y up, centred on the origin. verticesX/Z >= 2.
// Winding matches PrimitiveBuilder::buildPlane (normals face +Y).
OpStatus makeGrid(float sizeX, float sizeZ, int32_t verticesX, int32_t verticesZ,
                  const Limits& limits, Mesh& out);

// p' = T * R * S * p. rotationDegrees is Euler XYZ (R = Rz * Ry * Rx, as Blender).
// Normals use the inverse-transpose of the linear part, so non-uniform scale is
// handled; when the linear part has negative determinant the triangle winding is
// reversed so faces still point outwards. Zero/near-zero scale is rejected.
OpStatus transformMesh(const Mesh& in, const Vec3& translation, const Vec3& rotationDegrees,
                       const Vec3& scale, Mesh& out);

// Concatenates meshes (index offsets are range-checked in 64 bits first).
// Attribute rule: the output has normals if any input has them, UVs if any input
// has them; an input lacking normals gets area-weighted smooth normals computed
// from its triangles, an input lacking UVs gets (0,0).
OpStatus joinMeshes(const std::vector<GeometryPtr>& inputs, const Limits& limits, Mesh& out);

// Phase 5. Points are a Mesh with positions (+ optional normals) and no indices.

// `count` points spread over the triangles proportionally to their area, a pure function of
// (contextSeed, nodeSeed, point index); normals are the face normals. No triangles / zero area
// yield an empty point set.
OpStatus distributePointsOnFaces(const Mesh& in, int32_t count, uint32_t contextSeed, int32_t nodeSeed,
                                 const Limits& limits, const std::atomic<bool>* cancel, Mesh& out);

// One instance of `source` per selected point (empty `selected` = all), placed at the point with the
// per-point rotation (Euler XYZ degrees) / scale (empty = zero / one). The output holds only instances.
OpStatus instanceOnPoints(const Mesh& points, const GeometryPtr& source, const std::vector<uint8_t>& selected,
                          const std::vector<Vec3>& rotations, const std::vector<Vec3>& scales,
                          const Limits& limits, Mesh& out);

// The mesh's own geometry joined with every instance transformed into place. Instances of instances
// are rejected at creation, so one level is enough.
OpStatus realizeInstances(const Mesh& in, const Limits& limits, const std::atomic<bool>* cancel, Mesh& out);

// Height-field terrain on the XZ plane (+Y up, centred on the origin, buildPlane() topology/winding):
// seeded gradient-noise fBm. This is the algorithm of the former Phantom::Terrain generatorVersion 1, so a
// recipe reproduces the same mesh. `seed` is the 32-bit permutation seed stored as int32 (bit pattern).
inline constexpr int32_t kMaxTerrainSegmentsPerAxis = 4096;
struct TerrainParams {
    float width = 10.0f;
    float depth = 10.0f;
    int32_t segmentsX = 128;
    int32_t segmentsZ = 128;
    float heightScale = 2.0f;
    float frequency = 0.15f;
    int32_t octaves = 5;
    float lacunarity = 2.0f;
    float persistence = 0.5f;
    int32_t seed = 0;
    float heightOffset = 0.0f;
};
// InvalidArgument for out-of-range / non-finite values, LimitExceeded for a grid over the limits.
OpStatus validateTerrain(const TerrainParams& params, const Limits& limits);
OpStatus makeTerrain(const TerrainParams& params, const Limits& limits, Mesh& out);

// Area-weighted vertex normals (zero-area vertices get +Y).
std::vector<Vec3> computeSmoothNormals(const Mesh& mesh);

}  // namespace Phantom::GeometryNode
