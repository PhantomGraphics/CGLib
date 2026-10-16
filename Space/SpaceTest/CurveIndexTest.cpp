// Boundary and exhaustive tests for the Morton (Z-order) encoder and the linear octree index.
// ZOrderCurve3d packs 10 bits per axis into 30 bits; LinearOctreeIndex is the cumulative-level numbering.
#include "gtest/gtest.h"

#include "../Core/LinearOctreeIndex.h"
#include "../Core/ZOrderCurve3d.h"

#include <random>

using namespace Phantom::Space;

namespace {
	constexpr unsigned int kMaxAxis = 1023; // 10 bits
	// Axis values below 256 only exercise the low byte; the full-range tests cover bits 8-9 as well
	// (decode() used to drop them because its final fold step was commented out).
	constexpr unsigned int kDecodableMax = 255;
}

TEST(CurveIndexTest, MortonRoundtripAtDecodableAxisLimits)
{
	for (const unsigned int x : { 0u, 1u, 127u, 128u, kDecodableMax })
		for (const unsigned int y : { 0u, 1u, 127u, 128u, kDecodableMax })
			for (const unsigned int z : { 0u, 1u, 127u, 128u, kDecodableMax }) {
				const auto code = ZOrderCurve3d::encode({ x, y, z });
				EXPECT_LT(code, 1u << 30);
				const auto back = ZOrderCurve3d::decode(code);
				EXPECT_EQ(back[0], x);
				EXPECT_EQ(back[1], y);
				EXPECT_EQ(back[2], z);
			}
}

TEST(CurveIndexTest, MortonEncodeUsesAtMost30BitsAndRoundtripsFullRange)
{
	std::mt19937 rng(100u);
	std::uniform_int_distribution<unsigned int> dist(0, kDecodableMax);
	for (int i = 0; i < 5000; ++i) {
		const std::array<unsigned int, 3> idx{ dist(rng), dist(rng), dist(rng) };
		EXPECT_EQ(ZOrderCurve3d::decode(ZOrderCurve3d::encode(idx)), idx);
	}
}

TEST(CurveIndexTest, MortonDecodeRoundtripsFullTenBitRange)
{
	std::mt19937 rng(101u);
	std::uniform_int_distribution<unsigned int> dist(0, kMaxAxis);
	for (int i = 0; i < 5000; ++i) {
		const std::array<unsigned int, 3> idx{ dist(rng), dist(rng), dist(rng) };
		EXPECT_EQ(ZOrderCurve3d::decode(ZOrderCurve3d::encode(idx)), idx);
	}
	for (const unsigned int v : { 256u, 511u, 512u, kMaxAxis })
		EXPECT_EQ(ZOrderCurve3d::decode(ZOrderCurve3d::encode({ v, v, v })), (std::array<unsigned int, 3>{ v, v, v }));
}

TEST(CurveIndexTest, MortonIsInjectiveOnSmallCube)
{
	// 16^3 cells -> 4096 distinct codes, so no two cells can alias.
	std::vector<bool> seen(1u << 12, false);
	for (unsigned int x = 0; x < 16; ++x)
		for (unsigned int y = 0; y < 16; ++y)
			for (unsigned int z = 0; z < 16; ++z) {
				const auto code = ZOrderCurve3d::encode({ x, y, z });
				ASSERT_LT(code, seen.size());
				EXPECT_FALSE(seen[code]) << x << "," << y << "," << z;
				seen[code] = true;
			}
}

TEST(CurveIndexTest, MortonBitLayoutIsXYZInterleaved)
{
	// x occupies bit 0, y bit 1, z bit 2 of each 3-bit group.
	EXPECT_EQ(ZOrderCurve3d::encode({ 1, 0, 0 }), 1u);
	EXPECT_EQ(ZOrderCurve3d::encode({ 0, 1, 0 }), 2u);
	EXPECT_EQ(ZOrderCurve3d::encode({ 0, 0, 1 }), 4u);
	EXPECT_EQ(ZOrderCurve3d::encode({ 2, 0, 0 }), 8u);
	EXPECT_EQ(ZOrderCurve3d::encode({ kMaxAxis, kMaxAxis, kMaxAxis }), (1u << 30) - 1u);
}

TEST(CurveIndexTest, LinearOctreeLevelStartsAreCumulativePowersOfEight)
{
	unsigned int expectedStart = 0;
	unsigned int levelSize = 1;
	for (unsigned int level = 0; level <= 7; ++level) {
		EXPECT_EQ(LinearOctreeIndex(level, 0).getIndex1d(), expectedStart) << "level " << level;
		// Last cell of this level is immediately before the first cell of the next level.
		const auto last = LinearOctreeIndex(level, levelSize - 1);
		EXPECT_EQ(last.getIndex1d(), expectedStart + levelSize - 1) << "level " << level;
		expectedStart += levelSize;
		levelSize *= 8;
	}
}

TEST(CurveIndexTest, LinearOctreeLevelNumberRoundtripAtLevelBoundaries)
{
	unsigned int levelSize = 1;
	for (unsigned int level = 0; level <= 6; ++level) {
		for (const unsigned int number : { 0u, 1u, levelSize / 2, levelSize - 1 }) {
			if (number >= levelSize) continue; // level 0 has a single cell
			const LinearOctreeIndex index(level, number);
			const auto ln = index.getLevelAndNumber();
			EXPECT_EQ(ln.first, level);
			EXPECT_EQ(ln.second, number);
		}
		levelSize *= 8;
	}
}

TEST(CurveIndexTest, LinearOctreeIndex1dIsMonotonicAndBijectiveToLevelNumber)
{
	unsigned int expected = 0;
	for (unsigned int level = 0; level <= 3; ++level) {
		unsigned int levelSize = 1;
		for (unsigned int i = 0; i < level; ++i) levelSize *= 8;
		for (unsigned int number = 0; number < levelSize; ++number, ++expected) {
			const LinearOctreeIndex fromParts(level, number);
			const LinearOctreeIndex from1d(expected);
			EXPECT_EQ(fromParts, from1d);
			const auto ln = from1d.getLevelAndNumber();
			ASSERT_EQ(ln.first, level);
			ASSERT_EQ(ln.second, number);
		}
	}
}

TEST(CurveIndexTest, LinearOctreeParentRelation)
{
	// Every child of every level-2 cell points back to that cell.
	for (unsigned int parentNumber = 0; parentNumber < 64; ++parentNumber) {
		const LinearOctreeIndex parent(2, parentNumber);
		for (unsigned int c = 0; c < 8; ++c) {
			const LinearOctreeIndex child(3, parentNumber * 8 + c);
			EXPECT_EQ(child.getParentIndex(), parent) << parentNumber << "/" << c;
		}
	}
	// Level 1 cells all have the root as parent.
	for (unsigned int n = 0; n < 8; ++n) {
		EXPECT_EQ(LinearOctreeIndex(1, n).getParentIndex(), LinearOctreeIndex(0, 0));
	}
}
