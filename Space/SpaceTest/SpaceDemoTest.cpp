// Phase 5: the SpaceView demos' CPU side (SpaceDemos.h) checked without a window. The ImGui panels and the
// scenario commands both run these functions, so what is verified here is what the viewer draws.
#include "gtest/gtest.h"

#include "../SpaceView/SpaceDemos.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace Phantom::VKSpace;

namespace {
	constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
	constexpr float kInf = std::numeric_limits<float>::infinity();

	std::vector<glm::vec3> lattice(const glm::vec3& mn, const glm::vec3& mx, const int n)
	{
		std::vector<glm::vec3> pts;
		for (int ix = 0; ix < n; ++ix)
			for (int iy = 0; iy < n; ++iy)
				for (int iz = 0; iz < n; ++iz)
					pts.emplace_back(mn.x + (mx.x - mn.x) * (ix + 0.5f) / n,
						mn.y + (mx.y - mn.y) * (iy + 0.5f) / n,
						mn.z + (mx.z - mn.z) * (iz + 0.5f) / n);
		return pts;
	}

	float dist2(const glm::vec3& a, const glm::vec3& b) { const glm::vec3 d = a - b; return glm::dot(d, d); }

	bool allFinite(const std::vector<float>& v)
	{
		return std::all_of(v.begin(), v.end(), [](float x) { return std::isfinite(x); });
	}
}

// ---------------------------------------------------------------- parameter parsing

TEST(SpaceDemoTest, ParseRejectsNonFiniteAndTrailingGarbage)
{
	float f = 7.0f;
	for (const char* bad : { "", "abc", "nan", "NaN", "inf", "-inf", "1.0x", "1 ", " 1", "--1", "1e999" }) {
		EXPECT_FALSE(parseDemoFloat(bad, f)) << "'" << bad << "'";
	}
	EXPECT_EQ(f, 7.0f) << "output untouched on failure";
	ASSERT_TRUE(parseDemoFloat("0.10", f));
	EXPECT_FLOAT_EQ(f, 0.10f);
	ASSERT_TRUE(parseDemoFloat("-2.5e-1", f));
	EXPECT_FLOAT_EQ(f, -0.25f);

	int i = 9;
	for (const char* bad : { "", "x", "5abc", "1.5", "99999999999", " 3" }) {
		EXPECT_FALSE(parseDemoInt(bad, i)) << "'" << bad << "'";
	}
	EXPECT_EQ(i, 9);
	ASSERT_TRUE(parseDemoInt("-4", i));
	EXPECT_EQ(i, -4);
}

TEST(SpaceDemoTest, SetClampsValuesAndRejectsUnknownNamesOrBadText)
{
	HashDemoParams hash;
	EXPECT_TRUE(hash.set("SearchRadius", "0.10"));
	EXPECT_FLOAT_EQ(hash.searchRadius, 0.10f);
	EXPECT_TRUE(hash.set("SearchRadius", "1000"));
	EXPECT_FLOAT_EQ(hash.searchRadius, HashDemoParams::kMaxRadius);
	EXPECT_TRUE(hash.set("SearchRadius", "-3"));
	EXPECT_FLOAT_EQ(hash.searchRadius, HashDemoParams::kMinRadius);
	const float before = hash.searchRadius;
	EXPECT_FALSE(hash.set("SearchRadius", "nan"));
	EXPECT_FALSE(hash.set("Radius", "0.5"));
	EXPECT_FLOAT_EQ(hash.searchRadius, before);

	KDTreeDemoParams kd;
	EXPECT_TRUE(kd.set("k", "1000000"));
	EXPECT_EQ(kd.k, KDTreeDemoParams::kMaxK);
	EXPECT_TRUE(kd.set("k", "-5"));
	EXPECT_EQ(kd.k, KDTreeDemoParams::kMinK);
	EXPECT_FALSE(kd.set("k", "2.5"));

	OctreeDemoParams oc;
	EXPECT_TRUE(oc.set("MaxDepth", "100"));
	EXPECT_EQ(oc.maxDepth, OctreeDemoParams::kMaxDepth);
	EXPECT_FALSE(oc.set("Depth", "2"));

	SignedDistanceDemoParams sd;
	EXPECT_TRUE(sd.set("Samples", "100000"));
	EXPECT_EQ(sd.samples, SignedDistanceDemoParams::kMaxSamples);
	EXPECT_TRUE(sd.set("SphereRadius", "50"));
	EXPECT_FLOAT_EQ(sd.sphereRadius, SignedDistanceDemoParams::kMaxRadius);
	EXPECT_FALSE(sd.set("Samples", "inf"));
}

TEST(SpaceDemoTest, SanitizeRepairsNonFiniteFieldValues)
{
	HashDemoParams hash;
	hash.searchRadius = kNaN;
	hash.sanitize();
	EXPECT_FLOAT_EQ(hash.searchRadius, HashDemoParams{}.searchRadius);
	hash.searchRadius = kInf;
	hash.sanitize();
	EXPECT_FLOAT_EQ(hash.searchRadius, HashDemoParams{}.searchRadius);

	SignedDistanceDemoParams sd;
	sd.sphereRadius = kNaN;
	sd.samples = std::numeric_limits<int>::max();
	sd.sanitize();
	EXPECT_FLOAT_EQ(sd.sphereRadius, SignedDistanceDemoParams{}.sphereRadius);
	EXPECT_EQ(sd.samples, SignedDistanceDemoParams::kMaxSamples);

	// A hostile parameter struct must still run to a finite, bounded result.
	SpaceResult out;
	hash.searchRadius = kNaN;
	const auto stats = runSpaceHashDemo(hash, out);
	EXPECT_GE(stats.totalNeighbors, 216) << "at least every point finds itself";
	EXPECT_TRUE(allFinite(out.linePositions));
}

// ---------------------------------------------------------------- SpaceHash / CompactSpaceHash

TEST(SpaceDemoTest, HashDemosMatchBruteForceNeighborCounts)
{
	for (const float radius : { 0.05f, 0.2f, 0.35f, 0.8f, 1.5f }) {
		HashDemoParams params;
		params.searchRadius = radius;
		const float r = std::max(radius, HashDemoParams::kMinRadius);

		// SpaceHash demo lattice lives in [-1,1]^3, the compact one in [0,1]^3; both count the point itself.
		const struct { glm::vec3 mn, mx; bool compact; } cases[] = {
			{ glm::vec3(-1.f), glm::vec3(1.f), false }, { glm::vec3(0.f), glm::vec3(1.f), true } };
		for (const auto& c : cases) {
			const auto pts = lattice(c.mn, c.mx, 6);
			int expected = 0;
			int markers = 0;
			for (const auto& p : pts) {
				int count = 0;
				for (const auto& q : pts) if (dist2(p, q) < r * r) ++count;
				expected += count;
				if (c.compact ? count > 0 : count > 1) ++markers;
			}
			SpaceResult out;
			const auto stats = c.compact ? runCompactSpaceHashDemo(params, out) : runSpaceHashDemo(params, out);
			EXPECT_EQ(stats.totalNeighbors, expected) << "radius " << radius << (c.compact ? " compact" : "");
			EXPECT_EQ(out.lineIndices.size(), static_cast<size_t>(markers) * 24) << "one 12-edge marker per qualifying point";
			EXPECT_TRUE(allFinite(out.linePositions));
		}
	}
}

// ---------------------------------------------------------------- KDTree

TEST(SpaceDemoTest, KDTreeDemoLinksEachPointToItsKNearest)
{
	for (const int k : { 1, 3, 8, 26, 64 }) {
		KDTreeDemoParams params;
		params.k = k;
		SpaceResult out;
		const auto stats = runKDTreeDemo(params, out);

		const auto pts = lattice(glm::vec3(-1.f), glm::vec3(1.f), 5);
		const int perPoint = std::min(k, static_cast<int>(pts.size()) - 1);
		ASSERT_EQ(out.lineIndices.size(), pts.size() * (24 + 2 * static_cast<size_t>(perPoint))) << "k=" << k;
		EXPECT_EQ(stats.lineIndexCount, static_cast<int>(out.lineIndices.size()));

		// Walk the stream: per point one 8-vertex box (24 indices), then `perPoint` segments (2 indices each).
		size_t at = 0;
		auto vertex = [&](const uint32_t index) {
			return glm::vec3(out.linePositions[index * 3], out.linePositions[index * 3 + 1], out.linePositions[index * 3 + 2]);
		};
		for (size_t i = 0; i < pts.size(); ++i) {
			at += 24;
			std::vector<float> linked;
			for (int l = 0; l < perPoint; ++l, at += 2) {
				const glm::vec3 a = vertex(out.lineIndices[at]);
				const glm::vec3 b = vertex(out.lineIndices[at + 1]);
				EXPECT_NEAR(glm::length(a - pts[i]), 0.0f, 1.0e-6f) << "segment starts at the point";
				linked.push_back(dist2(a, b));
			}
			std::vector<float> all;
			for (size_t j = 0; j < pts.size(); ++j) if (j != i) all.push_back(dist2(pts[i], pts[j]));
			std::sort(all.begin(), all.end());
			std::sort(linked.begin(), linked.end());
			for (int l = 0; l < perPoint; ++l) {
				ASSERT_NEAR(linked[static_cast<size_t>(l)], all[static_cast<size_t>(l)], 1.0e-5f) << "point " << i << " k=" << k << " rank " << l;
			}
		}
	}
}

// ---------------------------------------------------------------- Octree

TEST(SpaceDemoTest, OctreeDemoDrawsEveryNodeUpToTheRequestedDepth)
{
	int expectedNodes = 0;
	int levelNodes = 1;
	for (int depth = 1; depth <= 4; ++depth) {
		expectedNodes += levelNodes; // levels 1..depth, the root being level 1
		levelNodes *= 8;
		OctreeDemoParams params;
		params.maxDepth = depth;
		SpaceResult out;
		const auto stats = runOctreeDemo(params, out);
		EXPECT_EQ(stats.nodeCount, expectedNodes) << "depth " << depth;
		EXPECT_EQ(out.lineIndices.size(), static_cast<size_t>(expectedNodes) * 24);
		// Every box lies inside the root cube.
		for (const float v : out.linePositions) {
			EXPECT_GE(v, -1.0f - 1.0e-6f);
			EXPECT_LE(v, 1.0f + 1.0e-6f);
		}
	}
}

TEST(SpaceDemoTest, OctreeDemoDepthIsBounded)
{
	// Depth is clamped before any work is done; check the limit without building 300k nodes twice.
	OctreeDemoParams params;
	params.maxDepth = std::numeric_limits<int>::max();
	params.sanitize();
	EXPECT_EQ(params.maxDepth, OctreeDemoParams::kMaxDepth);
	params.maxDepth = std::numeric_limits<int>::min();
	params.sanitize();
	EXPECT_EQ(params.maxDepth, OctreeDemoParams::kMinDepth);
}

// ---------------------------------------------------------------- SignedDistance

TEST(SpaceDemoTest, SignedDistanceDemoSamplesTheSphereField)
{
	for (const float radius : { 0.2f, 0.5f, 1.5f }) {
		SignedDistanceDemoParams params;
		params.sphereRadius = radius;
		params.samples = 10;
		SpaceResult out;
		const auto stats = runSignedDistanceDemo(params, out);
		EXPECT_EQ(stats.sampleCount, 1000);
		EXPECT_EQ(out.pointSizes.size(), 1000u);
		EXPECT_NEAR(stats.minSdf, -radius, 1.0e-5f) << "the grid contains the sphere centre (u = 0.5)";
		EXPECT_NEAR(stats.maxSdf, std::sqrt(3.0f) * radius - radius, 1.0e-4f) << "the corner of the bounding box";
		for (size_t i = 0; i < out.pointPositions.size(); ++i) {
			EXPECT_LE(std::abs(out.pointPositions[i]), radius + 1.0e-6f);
			EXPECT_GE(std::abs(out.pointPositions[i]), 0.0f);
		}
		// Inside is blue-ish, outside red-ish.
		for (size_t i = 0; i < out.pointSizes.size(); ++i) {
			const glm::vec3 p(out.pointPositions[i * 3], out.pointPositions[i * 3 + 1], out.pointPositions[i * 3 + 2]);
			const float r = out.pointColors[i * 4];
			const float b = out.pointColors[i * 4 + 2];
			if (glm::length(p) < radius - 1.0e-4f) EXPECT_GT(b, r);
			if (glm::length(p) > radius + 1.0e-4f) EXPECT_GT(r, b);
		}
	}
}

// ---------------------------------------------------------------- result handling

TEST(SpaceDemoTest, EveryRunReplacesThePreviousResultAndIsDeterministic)
{
	SpaceResult out;
	out.addLine(glm::vec3(0.f), glm::vec3(1.f), glm::vec4(1.f)); // stale content from an earlier run
	out.addPoint(glm::vec3(9.f), glm::vec4(1.f), 1.0f);

	runKDTreeDemo(KDTreeDemoParams{}, out);
	const auto first = out.linePositions;
	EXPECT_TRUE(out.pointSizes.empty()) << "points of the previous result are gone";
	runKDTreeDemo(KDTreeDemoParams{}, out);
	EXPECT_EQ(out.linePositions, first) << "running twice does not accumulate and gives the same geometry";

	runSignedDistanceDemo(SignedDistanceDemoParams{}, out);
	EXPECT_TRUE(out.lineIndices.empty()) << "lines of the previous demo are gone";
	EXPECT_FALSE(out.pointSizes.empty());

	// The demos leave the dirty flag to their caller (the panel marks the world dirty after a run).
	EXPECT_FALSE(out.dirty);
}
