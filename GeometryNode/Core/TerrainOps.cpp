#include "GeometryOps.h"
#include "Math/Noise/PerlinNoise.h"

// Height-field terrain (docs/todo/PLAN_geometry_node.md section 9). The noise and mesh construction are the
// algorithm of the former Phantom::Terrain module (generatorVersion 1), moved here unchanged so existing
// terrain recipes reproduce the same mesh: seeded Perlin-style gradient noise (permutation shuffled with
// splitmix32), fBm normalised by the amplitude sum, central-difference normals, buildPlane() topology.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace Phantom::GeometryNode {

namespace {

bool isFinite(float v) { return std::isfinite(v); }

}  // namespace

OpStatus validateTerrain(const TerrainParams& s, const Limits& limits) {
    if (!isFinite(s.width) || !isFinite(s.depth) || s.width <= 0.0f || s.depth <= 0.0f) return OpStatus::InvalidArgument;
    if (!isFinite(s.frequency) || s.frequency <= 0.0f) return OpStatus::InvalidArgument;
    if (!isFinite(s.heightScale) || s.heightScale < 0.0f) return OpStatus::InvalidArgument;
    if (!isFinite(s.heightOffset)) return OpStatus::InvalidArgument;
    if (s.octaves < 1 || s.octaves > 12) return OpStatus::InvalidArgument;
    if (!isFinite(s.lacunarity) || s.lacunarity <= 1.0f) return OpStatus::InvalidArgument;
    if (!isFinite(s.persistence) || s.persistence < 0.0f || s.persistence > 1.0f) return OpStatus::InvalidArgument;
    if (s.segmentsX < 1 || s.segmentsZ < 1) return OpStatus::InvalidArgument;
    if (s.segmentsX > kMaxTerrainSegmentsPerAxis || s.segmentsZ > kMaxTerrainSegmentsPerAxis) return OpStatus::LimitExceeded;

    // The noise floors its coordinates into int; keep every octave inside that range, and keep the grid
    // spacing / height slope representable for the float normal math.
    const double dx = static_cast<double>(s.width) / s.segmentsX;
    const double dz = static_cast<double>(s.depth) / s.segmentsZ;
    if (!(dx > 0.0) || !(dz > 0.0)) return OpStatus::InvalidArgument;
    constexpr double kMaxNoiseCoordinate = static_cast<double>(std::numeric_limits<int>::max()) - 2.0;
    double maxNoiseCoordinate = 0.5 * std::max(static_cast<double>(s.width), static_cast<double>(s.depth)) *
                                static_cast<double>(s.frequency);
    for (int32_t octave = 0; octave < s.octaves; ++octave) {
        if (!std::isfinite(maxNoiseCoordinate) || maxNoiseCoordinate > kMaxNoiseCoordinate) return OpStatus::InvalidArgument;
        maxNoiseCoordinate *= static_cast<double>(s.lacunarity);
    }
    const double maxAbsHeight = std::abs(static_cast<double>(s.heightOffset)) + 2.0 * static_cast<double>(s.heightScale);
    const double maxSlope = 2.0 * maxAbsHeight / std::min(dx, dz);
    const double safeSlope = 0.5 * std::sqrt(static_cast<double>(std::numeric_limits<float>::max()));
    if (!std::isfinite(maxAbsHeight) || !std::isfinite(maxSlope) || maxSlope > safeSlope) return OpStatus::InvalidArgument;

    const uint64_t vertices = (static_cast<uint64_t>(s.segmentsX) + 1u) * (static_cast<uint64_t>(s.segmentsZ) + 1u);
    const uint64_t indices = static_cast<uint64_t>(s.segmentsX) * static_cast<uint64_t>(s.segmentsZ) * 6u;
    if (!fitsLimits(vertices, indices, limits)) return OpStatus::LimitExceeded;
    return OpStatus::Ok;
}

OpStatus makeTerrain(const TerrainParams& s, const Limits& limits, Mesh& out) {
    if (OpStatus st = validateTerrain(s, limits); st != OpStatus::Ok) return st;

    const uint32_t sx = static_cast<uint32_t>(s.segmentsX);
    const uint32_t sz = static_cast<uint32_t>(s.segmentsZ);
    const uint32_t nvx = sx + 1;
    const uint32_t nvz = sz + 1;
    const size_t vertexCount = static_cast<size_t>(nvx) * static_cast<size_t>(nvz);

    const float halfW = s.width * 0.5f;
    const float halfD = s.depth * 0.5f;
    const float dx = s.width / static_cast<float>(sx);
    const float dz = s.depth / static_cast<float>(sz);

    const Phantom::Math::Noise::PerlinNoise noise(static_cast<uint32_t>(s.seed));

    // Pass 1: heights on the shared grid, so normals use identical neighbour values (no seams).
    std::vector<float> heights(vertexCount);
    for (uint32_t i = 0; i < nvz; ++i) {
        for (uint32_t j = 0; j < nvx; ++j) {
            const float wx = -halfW + static_cast<float>(j) * dx;
            const float wz = -halfD + static_cast<float>(i) * dz;
            const auto h = Phantom::Math::Noise::evaluateFractal([&](float f) {
                return noise.sample2D((wx * s.frequency) * f, (wz * s.frequency) * f);
            }, {Phantom::Math::Noise::FractalMode::FBm, static_cast<unsigned>(s.octaves), s.lacunarity, s.persistence});
            if (!h) return OpStatus::InvalidArgument;
            heights[static_cast<size_t>(i) * nvx + j] = s.heightOffset + s.heightScale * *h;
        }
    }

    Mesh mesh;
    mesh.positions.resize(vertexCount);
    mesh.normals.resize(vertexCount);
    mesh.uvs.resize(vertexCount);

    // Pass 2: positions, central-difference normals (one-sided at the border), grid UVs in [0, 1].
    for (uint32_t i = 0; i < nvz; ++i) {
        for (uint32_t j = 0; j < nvx; ++j) {
            const size_t idx = static_cast<size_t>(i) * nvx + j;
            const float y = heights[idx];

            const uint32_t jl = (j > 0) ? j - 1 : j;
            const uint32_t jr = (j < sx) ? j + 1 : j;
            const uint32_t id = (i > 0) ? i - 1 : i;
            const uint32_t iu = (i < sz) ? i + 1 : i;
            const float spanX = (j > 0 && j < sx) ? 2.0f * dx : dx;
            const float spanZ = (i > 0 && i < sz) ? 2.0f * dz : dz;
            const float dHdx = (heights[static_cast<size_t>(i) * nvx + jr] - heights[static_cast<size_t>(i) * nvx + jl]) / spanX;
            const float dHdz = (heights[static_cast<size_t>(iu) * nvx + j] - heights[static_cast<size_t>(id) * nvx + j]) / spanZ;

            float nx = -dHdx;
            float ny = 1.0f;
            float nz = -dHdz;
            const float invLen = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            nx *= invLen;
            ny *= invLen;
            nz *= invLen;

            mesh.positions[idx] = {-halfW + static_cast<float>(j) * dx, y, -halfD + static_cast<float>(i) * dz};
            mesh.normals[idx] = {nx, ny, nz};
            mesh.uvs[idx] = {static_cast<float>(j) / static_cast<float>(sx), static_cast<float>(i) / static_cast<float>(sz)};
        }
    }

    // Pass 3: indices -- the same (a,c,b)/(b,c,d) winding as makeGrid, CCW seen from +Y.
    mesh.indices.reserve(static_cast<size_t>(sx) * sz * 6);
    for (uint32_t i = 0; i < sz; ++i) {
        for (uint32_t j = 0; j < sx; ++j) {
            const uint32_t a = i * nvx + j;
            const uint32_t b = a + 1;
            const uint32_t c = (i + 1) * nvx + j;
            const uint32_t d = c + 1;
            for (uint32_t k : {a, c, b, b, c, d}) mesh.indices.push_back(k);
        }
    }

    out = std::move(mesh);
    return OpStatus::Ok;
}

}  // namespace Phantom::GeometryNode
