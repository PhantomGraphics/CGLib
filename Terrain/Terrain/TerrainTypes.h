#pragma once

// Phantom::Terrain -- CPU-only, GUI/Vulkan-independent height-field terrain
// generation. The public contract is "given a TerrainSettings recipe, produce a
// deterministic indexed triangle mesh"; the noise implementation is an internal
// detail (see TerrainGenerator.cpp).
//
// No dependency on MathCore / GLM / Vulkan / WPF / any JSON library -- the recipe
// serialization lives in the consuming application, not here (see
// docs/todo/PLAN_cgstudio_terrain_generation.md section 3.1 / section 8).

#include <cstdint>
#include <vector>

namespace Phantom::Terrain {

// Bumped only when a change to the generator would alter the mesh produced from
// an unchanged TerrainSettings. Persisted recipes store this so a scene can
// refuse to silently reinterpret an old terrain (plan section 4.3).
inline constexpr uint32_t kTerrainGeneratorVersion = 1;

// Practical upper bound on grid resolution. The MVP UI caps input at 1024
// segments per axis; the library allows headroom above that but still rejects
// anything that would blow past uint32_t indices or a sane memory budget.
inline constexpr uint32_t kMaxSegmentsPerAxis = 4096;

// Ceiling on total vertices, cross-checked against the uint32_t index space.
inline constexpr uint64_t kMaxVertexCount = 32u * 1024u * 1024u;

struct TerrainSettings {
    float width = 10.0f;        // X extent in world units (> 0)
    float depth = 10.0f;        // Z extent in world units (> 0)
    uint32_t segmentsX = 128;   // quad columns (>= 1, <= kMaxSegmentsPerAxis)
    uint32_t segmentsZ = 128;   // quad rows    (>= 1, <= kMaxSegmentsPerAxis)
    float heightScale = 2.0f;   // fBm amplitude multiplier (>= 0)
    float frequency = 0.15f;    // base spatial frequency (> 0)
    uint32_t octaves = 5;       // fBm octave count (1..12)
    float lacunarity = 2.0f;    // per-octave frequency multiplier (> 1)
    float persistence = 0.5f;   // per-octave amplitude multiplier (0..1)
    uint32_t seed = 0;          // permutation-table seed (any value)
    float heightOffset = 0.0f;  // constant added to every vertex height
};

struct TerrainVertex {
    float position[3];
    float normal[3];
    float uv[2];
};

struct TerrainMesh {
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
};

enum class TerrainError {
    None,
    InvalidSettings,  // a non-size parameter is out of range or non-finite
    MeshTooLarge,     // grid resolution exceeds the vertex / index budget
};

}  // namespace Phantom::Terrain
