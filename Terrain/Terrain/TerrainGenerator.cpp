#include "TerrainGenerator.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace Phantom::Terrain {

namespace {

// --- Deterministic PRNG (splitmix32) --------------------------------------
// Used only to shuffle the permutation table from `seed`. Kept in-module so
// the permutation, and therefore the noise field, is fixed by the public
// generatorVersion regardless of platform <random> differences.
class SplitMix32 {
public:
    explicit SplitMix32(uint32_t seed) : state_(seed) {}

    uint32_t next() {
        state_ += 0x9E3779B9u;
        uint32_t z = state_;
        z = (z ^ (z >> 16)) * 0x21F0AAADu;
        z = (z ^ (z >> 15)) * 0x735A2D97u;
        return z ^ (z >> 15);
    }

private:
    uint32_t state_;
};

// --- Seeded 2D gradient noise -------------------------------------------------
// Classic Perlin-style value in roughly [-1, 1]. The seed selects the
// permutation via a Fisher-Yates shuffle; a plain coordinate offset is
// deliberately avoided (plan section 3.1).
class GradientNoise2D {
public:
    explicit GradientNoise2D(uint32_t seed) {
        std::array<uint8_t, 256> p{};
        for (int i = 0; i < 256; ++i) {
            p[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
        }
        SplitMix32 rng(seed);
        for (int i = 255; i > 0; --i) {
            const uint32_t j = rng.next() % static_cast<uint32_t>(i + 1);
            const uint8_t tmp = p[static_cast<size_t>(i)];
            p[static_cast<size_t>(i)] = p[j];
            p[j] = tmp;
        }
        for (size_t i = 0; i < 256; ++i) {
            perm_[i] = perm_[i + 256] = p[i];
        }
    }

    float sample(float x, float y) const {
        const float fx = std::floor(x);
        const float fy = std::floor(y);
        const int xi = static_cast<int>(fx) & 255;
        const int yi = static_cast<int>(fy) & 255;
        const float xf = x - fx;
        const float yf = y - fy;

        const float u = fade(xf);
        const float v = fade(yf);

        const int aa = perm_[static_cast<size_t>(perm_[static_cast<size_t>(xi)] + yi)];
        const int ab = perm_[static_cast<size_t>(perm_[static_cast<size_t>(xi)] + yi + 1)];
        const int ba = perm_[static_cast<size_t>(perm_[static_cast<size_t>(xi + 1)] + yi)];
        const int bb = perm_[static_cast<size_t>(perm_[static_cast<size_t>(xi + 1)] + yi + 1)];

        const float x1 = lerp(grad(aa, xf, yf), grad(ba, xf - 1.0f, yf), u);
        const float x2 = lerp(grad(ab, xf, yf - 1.0f), grad(bb, xf - 1.0f, yf - 1.0f), u);
        return lerp(x1, x2, v);
    }

private:
    static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float lerp(float a, float b, float t) { return a + t * (b - a); }
    static float grad(int hash, float x, float y) {
        switch (hash & 7) {
            case 0: return x + y;
            case 1: return -x + y;
            case 2: return x - y;
            case 3: return -x - y;
            case 4: return x;
            case 5: return -x;
            case 6: return y;
            default: return -y;
        }
    }

    std::array<uint8_t, 512> perm_{};
};

// fBm normalised by the amplitude sum so heightScale keeps its meaning as the
// octave count changes (plan section 4.1).
float fbm(const GradientNoise2D& noise, float x, float y, uint32_t octaves,
         float lacunarity, float persistence) {
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float sum = 0.0f;
    float amplitudeSum = 0.0f;
    for (uint32_t o = 0; o < octaves; ++o) {
        sum += amplitude * noise.sample(x * frequency, y * frequency);
        amplitudeSum += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }
    return amplitudeSum > 0.0f ? sum / amplitudeSum : 0.0f;
}

bool isFinite(float v) { return std::isfinite(v); }

}  // namespace

TerrainError validate(const TerrainSettings& s) {
    if (!isFinite(s.width) || !isFinite(s.depth) || s.width <= 0.0f || s.depth <= 0.0f) {
        return TerrainError::InvalidSettings;
    }
    if (!isFinite(s.frequency) || s.frequency <= 0.0f) {
        return TerrainError::InvalidSettings;
    }
    if (!isFinite(s.heightScale) || s.heightScale < 0.0f) {
        return TerrainError::InvalidSettings;
    }
    if (!isFinite(s.heightOffset)) {
        return TerrainError::InvalidSettings;
    }
    if (s.octaves < 1 || s.octaves > 12) {
        return TerrainError::InvalidSettings;
    }
    if (!isFinite(s.lacunarity) || s.lacunarity <= 1.0f) {
        return TerrainError::InvalidSettings;
    }
    if (!isFinite(s.persistence) || s.persistence < 0.0f || s.persistence > 1.0f) {
        return TerrainError::InvalidSettings;
    }
    if (s.segmentsX < 1 || s.segmentsZ < 1) {
        return TerrainError::InvalidSettings;
    }
    if (s.segmentsX > kMaxSegmentsPerAxis || s.segmentsZ > kMaxSegmentsPerAxis) {
        return TerrainError::MeshTooLarge;
    }

    // Size math in 64-bit so a huge grid is rejected, never wrapped.
    const uint64_t vx = static_cast<uint64_t>(s.segmentsX) + 1u;
    const uint64_t vz = static_cast<uint64_t>(s.segmentsZ) + 1u;
    const uint64_t vertexCount = vx * vz;
    if (vertexCount > kMaxVertexCount || vertexCount > 0xFFFFFFFFull) {
        return TerrainError::MeshTooLarge;
    }
    const uint64_t indexCount =
        static_cast<uint64_t>(s.segmentsX) * static_cast<uint64_t>(s.segmentsZ) * 6u;
    if (indexCount > 0xFFFFFFFFull) {
        return TerrainError::MeshTooLarge;
    }
    return TerrainError::None;
}

TerrainError generate(const TerrainSettings& s, TerrainMesh& out) {
    const TerrainError err = validate(s);
    if (err != TerrainError::None) {
        return err;
    }

    const uint32_t sx = s.segmentsX;
    const uint32_t sz = s.segmentsZ;
    const uint32_t nvx = sx + 1;
    const uint32_t nvz = sz + 1;
    const size_t vertexCount = static_cast<size_t>(nvx) * static_cast<size_t>(nvz);

    const float halfW = s.width * 0.5f;
    const float halfD = s.depth * 0.5f;
    const float dx = s.width / static_cast<float>(sx);
    const float dz = s.depth / static_cast<float>(sz);

    const GradientNoise2D noise(s.seed);

    // Pass 1: heights on the shared grid, so normals use identical neighbour
    // values and no seam appears between quads.
    std::vector<float> heights(vertexCount);
    for (uint32_t i = 0; i < nvz; ++i) {
        for (uint32_t j = 0; j < nvx; ++j) {
            const float wx = -halfW + static_cast<float>(j) * dx;
            const float wz = -halfD + static_cast<float>(i) * dz;
            const float h = fbm(noise, wx * s.frequency, wz * s.frequency, s.octaves,
                                s.lacunarity, s.persistence);
            heights[static_cast<size_t>(i) * nvx + j] = s.heightOffset + s.heightScale * h;
        }
    }

    TerrainMesh mesh;
    mesh.vertices.resize(vertexCount);

    // Pass 2: positions, central-difference normals (one-sided at the border),
    // normalised grid UVs in [0, 1].
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
            const float dHdx =
                (heights[static_cast<size_t>(i) * nvx + jr] - heights[static_cast<size_t>(i) * nvx + jl]) / spanX;
            const float dHdz =
                (heights[static_cast<size_t>(iu) * nvx + j] - heights[static_cast<size_t>(id) * nvx + j]) / spanZ;

            float nx = -dHdx;
            float ny = 1.0f;
            float nz = -dHdz;
            const float invLen = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            nx *= invLen;
            ny *= invLen;
            nz *= invLen;

            TerrainVertex& v = mesh.vertices[idx];
            v.position[0] = -halfW + static_cast<float>(j) * dx;
            v.position[1] = y;
            v.position[2] = -halfD + static_cast<float>(i) * dz;
            v.normal[0] = nx;
            v.normal[1] = ny;
            v.normal[2] = nz;
            v.uv[0] = static_cast<float>(j) / static_cast<float>(sx);
            v.uv[1] = static_cast<float>(i) / static_cast<float>(sz);
        }
    }

    // Pass 3: indices -- same (a,c,b)/(b,c,d) winding as buildPlane(), CCW seen
    // from +Y.
    mesh.indices.reserve(static_cast<size_t>(sx) * sz * 6);
    for (uint32_t i = 0; i < sz; ++i) {
        for (uint32_t j = 0; j < sx; ++j) {
            const uint32_t a = i * nvx + j;
            const uint32_t b = a + 1;
            const uint32_t c = (i + 1) * nvx + j;
            const uint32_t d = c + 1;
            mesh.indices.push_back(a);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(d);
        }
    }

    out = std::move(mesh);
    return TerrainError::None;
}

}  // namespace Phantom::Terrain
