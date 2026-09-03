#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "../Terrain/TerrainGenerator.h"

using namespace Phantom::Terrain;

namespace {

TerrainSettings defaultSettings() {
    TerrainSettings s;  // struct defaults are the MVP dialog defaults
    return s;
}

bool allFinite(const TerrainMesh& mesh) {
    for (const auto& v : mesh.vertices) {
        for (float c : v.position) {
            if (!std::isfinite(c)) return false;
        }
        for (float c : v.normal) {
            if (!std::isfinite(c)) return false;
        }
        for (float c : v.uv) {
            if (!std::isfinite(c)) return false;
        }
    }
    return true;
}

}  // namespace

// --- validation -------------------------------------------------------------

TEST(TerrainGenerator, ValidateAcceptsDefaults) {
    EXPECT_EQ(validate(defaultSettings()), TerrainError::None);
}

TEST(TerrainGenerator, ValidateRejectsNonPositiveExtent) {
    TerrainSettings s = defaultSettings();
    s.width = 0.0f;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.depth = -1.0f;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
}

TEST(TerrainGenerator, ValidateRejectsNonFiniteFloats) {
    TerrainSettings s = defaultSettings();
    s.width = std::nanf("");
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.frequency = std::numeric_limits<float>::infinity();
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.heightOffset = std::nanf("");
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
}

TEST(TerrainGenerator, ValidateRejectsFrequencyAndHeightScaleBounds) {
    TerrainSettings s = defaultSettings();
    s.frequency = 0.0f;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.heightScale = -0.01f;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.heightScale = 0.0f;  // flat is legal
    EXPECT_EQ(validate(s), TerrainError::None);
}

TEST(TerrainGenerator, ValidateRejectsOctaveBounds) {
    TerrainSettings s = defaultSettings();
    s.octaves = 0;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s.octaves = 13;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s.octaves = 1;
    EXPECT_EQ(validate(s), TerrainError::None);
    s.octaves = 12;
    EXPECT_EQ(validate(s), TerrainError::None);
}

TEST(TerrainGenerator, ValidateRejectsLacunarityAndPersistenceBounds) {
    TerrainSettings s = defaultSettings();
    s.lacunarity = 1.0f;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.lacunarity = 0.5f;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.persistence = -0.1f;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.persistence = 1.1f;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.persistence = 0.0f;
    EXPECT_EQ(validate(s), TerrainError::None);
}

TEST(TerrainGenerator, ValidateRejectsZeroSegments) {
    TerrainSettings s = defaultSettings();
    s.segmentsX = 0;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
    s = defaultSettings();
    s.segmentsZ = 0;
    EXPECT_EQ(validate(s), TerrainError::InvalidSettings);
}

TEST(TerrainGenerator, ValidateRejectsOversizedGrid) {
    TerrainSettings s = defaultSettings();
    s.segmentsX = kMaxSegmentsPerAxis + 1;
    EXPECT_EQ(validate(s), TerrainError::MeshTooLarge);
    s = defaultSettings();
    s.segmentsZ = 4000000000u;  // near uint32_t max -- must not wrap
    EXPECT_EQ(validate(s), TerrainError::MeshTooLarge);
}

TEST(TerrainGenerator, ValidateAllowsMaxGrid) {
    TerrainSettings s = defaultSettings();
    s.segmentsX = kMaxSegmentsPerAxis;
    s.segmentsZ = kMaxSegmentsPerAxis;
    EXPECT_EQ(validate(s), TerrainError::None);
}

TEST(TerrainGenerator, GenerateLeavesOutputUntouchedOnInvalidInput) {
    TerrainSettings s = defaultSettings();
    s.segmentsX = 0;

    TerrainMesh mesh;
    mesh.vertices.resize(3);
    mesh.indices = {7, 8, 9};

    EXPECT_EQ(generate(s, mesh), TerrainError::InvalidSettings);
    EXPECT_EQ(mesh.vertices.size(), 3u);
    ASSERT_EQ(mesh.indices.size(), 3u);
    EXPECT_EQ(mesh.indices[0], 7u);
}

// --- topology --------------------------------------------------------------

TEST(TerrainGenerator, GenerateProducesExpectedCounts) {
    TerrainSettings s = defaultSettings();
    s.segmentsX = 16;
    s.segmentsZ = 24;

    TerrainMesh mesh;
    ASSERT_EQ(generate(s, mesh), TerrainError::None);
    EXPECT_EQ(mesh.vertices.size(), static_cast<size_t>(17) * 25);
    EXPECT_EQ(mesh.indices.size(), static_cast<size_t>(16) * 24 * 6);

    for (uint32_t idx : mesh.indices) {
        EXPECT_LT(idx, mesh.vertices.size());
    }
}

TEST(TerrainGenerator, GenerateCentresTerrainOnOrigin) {
    TerrainSettings s = defaultSettings();
    s.width = 8.0f;
    s.depth = 6.0f;
    s.segmentsX = 10;
    s.segmentsZ = 10;
    s.heightScale = 3.0f;

    TerrainMesh mesh;
    ASSERT_EQ(generate(s, mesh), TerrainError::None);

    float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
    for (const auto& v : mesh.vertices) {
        minX = std::min(minX, v.position[0]);
        maxX = std::max(maxX, v.position[0]);
        minZ = std::min(minZ, v.position[2]);
        maxZ = std::max(maxZ, v.position[2]);
    }
    EXPECT_NEAR(minX, -4.0f, 1.0e-5f);
    EXPECT_NEAR(maxX, 4.0f, 1.0e-5f);
    EXPECT_NEAR(minZ, -3.0f, 1.0e-5f);
    EXPECT_NEAR(maxZ, 3.0f, 1.0e-5f);
}

TEST(TerrainGenerator, UvCoversUnitSquare) {
    TerrainSettings s = defaultSettings();
    s.segmentsX = 12;
    s.segmentsZ = 9;

    TerrainMesh mesh;
    ASSERT_EQ(generate(s, mesh), TerrainError::None);

    float minU = 1e9f, maxU = -1e9f, minV = 1e9f, maxV = -1e9f;
    for (const auto& v : mesh.vertices) {
        EXPECT_GE(v.uv[0], 0.0f);
        EXPECT_LE(v.uv[0], 1.0f);
        EXPECT_GE(v.uv[1], 0.0f);
        EXPECT_LE(v.uv[1], 1.0f);
        minU = std::min(minU, v.uv[0]);
        maxU = std::max(maxU, v.uv[0]);
        minV = std::min(minV, v.uv[1]);
        maxV = std::max(maxV, v.uv[1]);
    }
    EXPECT_FLOAT_EQ(minU, 0.0f);
    EXPECT_FLOAT_EQ(maxU, 1.0f);
    EXPECT_FLOAT_EQ(minV, 0.0f);
    EXPECT_FLOAT_EQ(maxV, 1.0f);
}

// --- flat terrain ---------------------------------------------------------

TEST(TerrainGenerator, FlatTerrainHasUpNormalsAndConstantHeight) {
    TerrainSettings s = defaultSettings();
    s.heightScale = 0.0f;
    s.heightOffset = 1.5f;
    s.segmentsX = 20;
    s.segmentsZ = 20;

    TerrainMesh mesh;
    ASSERT_EQ(generate(s, mesh), TerrainError::None);

    for (const auto& v : mesh.vertices) {
        EXPECT_FLOAT_EQ(v.position[1], 1.5f);
        EXPECT_NEAR(v.normal[0], 0.0f, 1.0e-6f);
        EXPECT_NEAR(v.normal[1], 1.0f, 1.0e-6f);
        EXPECT_NEAR(v.normal[2], 0.0f, 1.0e-6f);
    }
}

TEST(TerrainGenerator, FlatTerrainWindingFacesUp) {
    TerrainSettings s = defaultSettings();
    s.heightScale = 0.0f;
    s.segmentsX = 8;
    s.segmentsZ = 8;

    TerrainMesh mesh;
    ASSERT_EQ(generate(s, mesh), TerrainError::None);

    for (size_t t = 0; t < mesh.indices.size(); t += 3) {
        const auto& p0 = mesh.vertices[mesh.indices[t]].position;
        const auto& p1 = mesh.vertices[mesh.indices[t + 1]].position;
        const auto& p2 = mesh.vertices[mesh.indices[t + 2]].position;
        const float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        const float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        const float ny = e1[2] * e2[0] - e1[0] * e2[2];  // Y of e1 x e2
        EXPECT_GT(ny, 0.0f);
    }
}

// --- noisy terrain ------------------------------------------------------

TEST(TerrainGenerator, NoisyTerrainIsCleanGeometry) {
    TerrainSettings s = defaultSettings();
    s.heightScale = 40.0f;
    s.frequency = 0.5f;
    s.octaves = 8;
    s.segmentsX = 48;
    s.segmentsZ = 48;
    s.seed = 99;

    TerrainMesh mesh;
    ASSERT_EQ(generate(s, mesh), TerrainError::None);
    EXPECT_TRUE(allFinite(mesh));

    // Unit-length normals.
    for (const auto& v : mesh.vertices) {
        const float len = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] +
                                    v.normal[2] * v.normal[2]);
        EXPECT_NEAR(len, 1.0f, 1.0e-4f);
    }

    // No degenerate triangles (all have clearly non-zero area).
    for (size_t t = 0; t < mesh.indices.size(); t += 3) {
        const auto& p0 = mesh.vertices[mesh.indices[t]].position;
        const auto& p1 = mesh.vertices[mesh.indices[t + 1]].position;
        const auto& p2 = mesh.vertices[mesh.indices[t + 2]].position;
        const float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        const float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        const float cx = e1[1] * e2[2] - e1[2] * e2[1];
        const float cy = e1[2] * e2[0] - e1[0] * e2[2];
        const float cz = e1[0] * e2[1] - e1[1] * e2[0];
        const float area2 = std::sqrt(cx * cx + cy * cy + cz * cz);
        EXPECT_GT(area2, 1.0e-6f);
    }
}

TEST(TerrainGenerator, NoisyTerrainVariesHeight) {
    TerrainSettings s = defaultSettings();
    s.heightScale = 5.0f;
    s.segmentsX = 32;
    s.segmentsZ = 32;

    TerrainMesh mesh;
    ASSERT_EQ(generate(s, mesh), TerrainError::None);

    float minY = 1e9f, maxY = -1e9f;
    for (const auto& v : mesh.vertices) {
        minY = std::min(minY, v.position[1]);
        maxY = std::max(maxY, v.position[1]);
    }
    EXPECT_GT(maxY - minY, 0.1f);
}

// --- determinism ------------------------------------------------------

TEST(TerrainGenerator, SameSeedIsBitReproducible) {
    TerrainSettings s = defaultSettings();
    s.heightScale = 7.0f;
    s.segmentsX = 40;
    s.segmentsZ = 40;
    s.seed = 2026;

    TerrainMesh a, b;
    ASSERT_EQ(generate(s, a), TerrainError::None);
    ASSERT_EQ(generate(s, b), TerrainError::None);

    ASSERT_EQ(a.vertices.size(), b.vertices.size());
    ASSERT_EQ(a.indices.size(), b.indices.size());
    EXPECT_EQ(std::memcmp(a.vertices.data(), b.vertices.data(),
                          a.vertices.size() * sizeof(TerrainVertex)),
              0);
    EXPECT_EQ(a.indices, b.indices);
}

TEST(TerrainGenerator, DifferentSeedChangesHeights) {
    TerrainSettings s = defaultSettings();
    s.heightScale = 7.0f;
    s.segmentsX = 40;
    s.segmentsZ = 40;

    s.seed = 1;
    TerrainMesh a;
    ASSERT_EQ(generate(s, a), TerrainError::None);

    s.seed = 2;
    TerrainMesh b;
    ASSERT_EQ(generate(s, b), TerrainError::None);

    ASSERT_EQ(a.vertices.size(), b.vertices.size());
    size_t differing = 0;
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (a.vertices[i].position[1] != b.vertices[i].position[1]) {
            ++differing;
        }
    }
    EXPECT_GT(differing, a.vertices.size() / 2);
}

TEST(TerrainGenerator, GeneratorVersionIsOne) {
    EXPECT_EQ(kTerrainGeneratorVersion, 1u);
}

// Not a correctness test -- a repeatable timing harness for
// docs/todo/PLAN_cgstudio_terrain_generation.md Phase 5. Run with:
//   TerrainTest.exe --gtest_also_run_disabled_tests --gtest_filter=*Benchmark*
TEST(TerrainGenerator, DISABLED_Benchmark) {
    for (uint32_t seg : {128u, 512u, 1024u}) {
        TerrainSettings s;
        s.segmentsX = seg;
        s.segmentsZ = seg;
        s.heightScale = 3.0f;
        s.octaves = 6;

        constexpr int kRuns = 5;
        double bestMs = 1e30;
        TerrainMesh mesh;
        for (int r = 0; r < kRuns; ++r) {
            const auto t0 = std::chrono::steady_clock::now();
            ASSERT_EQ(generate(s, mesh), TerrainError::None);
            const auto t1 = std::chrono::steady_clock::now();
            bestMs = std::min(bestMs, std::chrono::duration<double, std::milli>(t1 - t0).count());
        }

        // Dominant working set: the vertex + index buffers plus the transient height grid.
        const double meshMiB =
            (mesh.vertices.size() * sizeof(TerrainVertex) +
             mesh.indices.size() * sizeof(uint32_t) +
             mesh.vertices.size() * sizeof(float)) / (1024.0 * 1024.0);

        std::printf("[benchmark] %4ux%-4u  %10zu verts  %11zu idx  best %8.2f ms  ~%.1f MiB\n",
                    seg, seg, mesh.vertices.size(), mesh.indices.size(), bestMs, meshMiB);
    }
}
