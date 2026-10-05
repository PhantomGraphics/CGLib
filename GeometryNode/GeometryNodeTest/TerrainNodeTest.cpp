#include "gtest/gtest.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../GeometryNode/GeometryOps.h"
#include "../GeometryNode/GraphEvaluator.h"

using namespace Phantom::GeometryNode;

namespace {

NodeId add(Graph& g, const char* type) { return addNode(g, type).id; }

uint64_t fnv(uint64_t h, const void* data, size_t bytes) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < bytes; ++i) h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

// Bit-exact digest of a mesh (positions, normals, uvs, indices).
uint64_t digest(const Mesh& m) {
    uint64_t h = 1469598103934665603ull;
    h = fnv(h, m.positions.data(), m.positions.size() * sizeof(Vec3));
    h = fnv(h, m.normals.data(), m.normals.size() * sizeof(Vec3));
    h = fnv(h, m.uvs.data(), m.uvs.size() * sizeof(Vec2));
    h = fnv(h, m.indices.data(), m.indices.size() * sizeof(uint32_t));
    return h;
}

// Parameter sets that were compared bit-for-bit against the former Phantom::Terrain generator (generatorVersion 1)
// before it was deleted; their digests are the frozen reference now.
struct LegacyCase {
    const char* name;
    TerrainParams params;
    uint64_t digest;  // MSVC bit pattern
};

std::vector<LegacyCase> legacyCases() {
    std::vector<LegacyCase> cases;
    TerrainParams p;
    cases.push_back({"defaults", p, 5192843624707731235ull});
    p.seed = 12345; p.segmentsX = 37; p.segmentsZ = 53; p.width = 25.5f; p.depth = 7.25f;
    cases.push_back({"odd grid", p, 14392506333757915474ull});
    p = TerrainParams{};
    p.octaves = 12; p.lacunarity = 2.7f; p.persistence = 0.8f; p.frequency = 0.5f; p.heightScale = 9.0f; p.heightOffset = -3.5f;
    cases.push_back({"12 octaves", p, 11468247552074274650ull});
    p = TerrainParams{};
    p.seed = static_cast<int32_t>(0xDEADBEEFu); p.octaves = 1; p.persistence = 0.0f;  // 32-bit seeds survive as their int32 bit pattern
    cases.push_back({"big seed", p, 6555976045359306570ull});
    p = TerrainParams{};
    p.segmentsX = 1; p.segmentsZ = 1;
    cases.push_back({"single quad", p, 13616270680078957128ull});
    return cases;
}

}  // namespace

TEST(TerrainNode, MatchesTheFormerGeneratorDigests) {
    for (const LegacyCase& c : legacyCases()) {
        Mesh m;
        ASSERT_EQ(makeTerrain(c.params, Limits{}, m), OpStatus::Ok) << c.name;
#if defined(_WIN32)
        EXPECT_EQ(digest(m), c.digest) << c.name;
#endif
    }
}

TEST(TerrainNode, GoldenDigestIsFrozen) {
    // The recipe -> mesh contract (generatorVersion 1) must not change silently; this survives the deletion of the legacy generator.
    Mesh m;
    ASSERT_EQ(makeTerrain(TerrainParams{}, Limits{}, m), OpStatus::Ok);
    EXPECT_EQ(m.positions.size(), 129u * 129u);
    EXPECT_EQ(m.indices.size(), 128u * 128u * 6u);
#if defined(_WIN32)
    // Bit-exact digests (MSVC); other toolchains may differ in the last float bits, so only the structure is pinned there.
    EXPECT_EQ(digest(m), 5192843624707731235ull);
#endif
    TerrainParams q;
    q.seed = 7;
    q.segmentsX = 16;
    q.segmentsZ = 9;
    q.octaves = 3;
    ASSERT_EQ(makeTerrain(q, Limits{}, m), OpStatus::Ok);
#if defined(_WIN32)
    EXPECT_EQ(digest(m), 16688812150222492815ull);
#endif
    EXPECT_EQ(m.positions.size(), 17u * 10u);
}

TEST(TerrainNode, WindingAndNormalsFaceUp) {
    Mesh m;
    TerrainParams p;
    p.heightScale = 0.0f;
    p.segmentsX = 4;
    p.segmentsZ = 4;
    ASSERT_EQ(makeTerrain(p, Limits{}, m), OpStatus::Ok);
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3 &a = m.positions[m.indices[t]], &b = m.positions[m.indices[t + 1]], &c = m.positions[m.indices[t + 2]];
        const float ny = (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z);  // y of (b-a)x(c-a)
        EXPECT_GT(ny, 0.0f);
    }
    for (const Vec3& n : m.normals) EXPECT_NEAR(n.y, 1.0f, 1e-6f);
}

TEST(TerrainNode, RejectsBadParametersAndOversizedGrids) {
    Mesh m;
    TerrainParams p;
    p.width = 0.0f;
    EXPECT_EQ(makeTerrain(p, Limits{}, m), OpStatus::InvalidArgument);
    p = TerrainParams{};
    p.octaves = 13;
    EXPECT_EQ(makeTerrain(p, Limits{}, m), OpStatus::InvalidArgument);
    p = TerrainParams{};
    p.lacunarity = 1.0f;
    EXPECT_EQ(makeTerrain(p, Limits{}, m), OpStatus::InvalidArgument);
    p = TerrainParams{};
    p.persistence = 1.5f;
    EXPECT_EQ(makeTerrain(p, Limits{}, m), OpStatus::InvalidArgument);
    p = TerrainParams{};
    p.segmentsX = 0;
    EXPECT_EQ(makeTerrain(p, Limits{}, m), OpStatus::InvalidArgument);
    p = TerrainParams{};
    p.heightScale = NAN;
    EXPECT_EQ(makeTerrain(p, Limits{}, m), OpStatus::InvalidArgument);
    p = TerrainParams{};
    p.segmentsX = 5000;
    EXPECT_EQ(makeTerrain(p, Limits{}, m), OpStatus::LimitExceeded);
    p = TerrainParams{};
    Limits small;
    small.maxVertices = 1000;  // 129 * 129 does not fit
    EXPECT_EQ(makeTerrain(p, small, m), OpStatus::LimitExceeded);
}

TEST(TerrainNode, WorksAsAGraphNodeAndRespectsSeed) {
    Graph g;
    const NodeId t = add(g, "Terrain");
    const NodeId out = add(g, "Output");
    setParam(*g.findNode(t), "SegmentsX", int32_t(8));
    setParam(*g.findNode(t), "SegmentsZ", int32_t(8));
    addLink(g, t, "Geometry", out, "Geometry");
    const EvalResult a = evaluateGraph(g, NodeRegistry::builtin(), EvalContext{});
    ASSERT_TRUE(a.success);
    EXPECT_EQ(a.geometry->positions.size(), 81u);
    setParam(*g.findNode(t), "Seed", int32_t(1));
    const EvalResult b = evaluateGraph(g, NodeRegistry::builtin(), EvalContext{});
    ASSERT_TRUE(b.success);
    EXPECT_NE(digest(*a.geometry), digest(*b.geometry));
    setParam(*g.findNode(t), "Octaves", int32_t(99));
    const EvalResult bad = evaluateGraph(g, NodeRegistry::builtin(), EvalContext{});
    EXPECT_FALSE(bad.success);
}
