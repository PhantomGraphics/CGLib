// Phase 2: invalid input (NaN/Inf, bad cell sizes, out-of-range cells and indices) must be rejected or
// reported -- never reach an undefined cast, a division by zero or an out-of-range access.
// Contracts: docs/spec/space_api_contracts.md.
#include "gtest/gtest.h"

#include "../Core/CompactSpaceHash.h"
#include "../Core/IndexedParticle.h"
#include "../Core/IndexedSortBasedSearcher.h"
#include "../Core/CSRNeighborList.h"
#include "../Core/SpaceHash.h"
#include "../Core/SpaceValidation.h"
#include "../Core/ZIndexedSearcher.h"
#include "../Core/ZOrderCurve3d.h"

#include <algorithm>
#include <limits>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {
	constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
	constexpr float kInf = std::numeric_limits<float>::infinity();

	std::vector<int> sorted(const NeighborIndexView& v)
	{
		std::vector<int> out(v.begin(), v.end());
		std::sort(out.begin(), out.end());
		return out;
	}

	template <typename C>
	std::vector<int> sortedOf(const C& c)
	{
		std::vector<int> out(c.begin(), c.end());
		std::sort(out.begin(), out.end());
		return out;
	}
}

// ---------------------------------------------------------------- Validation helpers

TEST(InputValidationTest, CellSizeAndRadiusPredicates)
{
	EXPECT_TRUE(Validation::isValidCellSize(0.5f));
	EXPECT_TRUE(Validation::isValidCellSize(1.0e-30));
	for (const float bad : { 0.0f, -1.0f, kNaN, kInf, -kInf }) EXPECT_FALSE(Validation::isValidCellSize(bad)) << bad;

	EXPECT_TRUE(Validation::isValidRadius(0.0f)); // zero is a valid (degenerate) radius, unlike a cell size
	EXPECT_TRUE(Validation::isValidRadius(3.0));
	for (const float bad : { -0.001f, kNaN, kInf }) EXPECT_FALSE(Validation::isValidRadius(bad)) << bad;
}

TEST(InputValidationTest, CellCoordinateRangeAndTruncation)
{
	int c = 0;
	EXPECT_TRUE(Validation::tryToCellCoordinate(2.9f, 1.0f, c));
	EXPECT_EQ(c, 2);
	EXPECT_TRUE(Validation::tryToCellCoordinate(-2.9f, 1.0f, c));
	EXPECT_EQ(c, -2) << "truncation toward zero, the long-standing convention";
	EXPECT_TRUE(Validation::tryToCellCoordinate(-0.5f, 1.0f, c));
	EXPECT_EQ(c, 0);

	EXPECT_FALSE(Validation::tryToCellCoordinate(kNaN, 1.0f, c));
	EXPECT_FALSE(Validation::tryToCellCoordinate(kInf, 1.0f, c));
	EXPECT_FALSE(Validation::tryToCellCoordinate(1.0e30f, 1.0f, c));
	volatile float zero = 0.0f; // keep the compiler from folding the division by a constant 0
	EXPECT_FALSE(Validation::tryToCellCoordinate(1.0f, static_cast<float>(zero), c)) << "x / 0 is infinite";
	EXPECT_FALSE(Validation::tryToCellCoordinate(0.0f, static_cast<float>(zero), c)) << "0 / 0 is NaN";
	// Just inside the limit is accepted.
	EXPECT_TRUE(Validation::tryToCellCoordinate(static_cast<double>(Validation::kMaxCellCoordinate) - 1.0, 1.0, c));
	EXPECT_FALSE(Validation::tryToCellCoordinate(static_cast<double>(Validation::kMaxCellCoordinate), 1.0, c));
}

// ---------------------------------------------------------------- IndexedParticle / searcher

TEST(InputValidationTest, GridIdIs64BitAndExactBeyondIntRange)
{
	// iz = 3000 -> 3000 * 2^20 = 3.1e9, past INT_MAX. Used to be signed overflow.
	IndexedParticle::GridId id = 0;
	ASSERT_TRUE(IndexedParticle::tryToGridId(Vector3df(5.0f, 7.0f, 3000.0f), 1.0f, id));
	EXPECT_EQ(id, static_cast<IndexedParticle::GridId>(3000) * (1 << 20) + 7 * (1 << 10) + 5);
	EXPECT_GT(id, static_cast<IndexedParticle::GridId>(std::numeric_limits<int>::max()));

	// Same value through the member API.
	IndexedParticle p(Vector3df(5.0f, 7.0f, 3000.0f));
	ASSERT_TRUE(p.trySetGridId(1.0f));
	EXPECT_EQ(p.getGridId(), id);

	// Negative cells: linear, so the id is simply negative.
	ASSERT_TRUE(IndexedParticle::tryToGridId(Vector3df(-2.0f, 0.0f, -1.0f), 1.0f, id));
	EXPECT_EQ(id, -(1 << 20) - 2);
}

TEST(InputValidationTest, TryToGridIdRejectsUnusableInput)
{
	IndexedParticle::GridId id = 123;
	EXPECT_FALSE(IndexedParticle::tryToGridId(Vector3df(kNaN, 0, 0), 1.0f, id));
	EXPECT_FALSE(IndexedParticle::tryToGridId(Vector3df(0, kInf, 0), 1.0f, id));
	EXPECT_FALSE(IndexedParticle::tryToGridId(Vector3df(0, 0, -kInf), 1.0f, id));
	EXPECT_FALSE(IndexedParticle::tryToGridId(Vector3df(1.0e20f, 0, 0), 1.0f, id));
	for (const float bad : { 0.0f, -1.0f, kNaN, kInf }) {
		EXPECT_FALSE(IndexedParticle::tryToGridId(Vector3df(1, 1, 1), bad, id)) << bad;
	}
	EXPECT_EQ(id, 123) << "output must be untouched on failure";

	IndexedParticle p(Vector3df(kNaN, 0, 0));
	EXPECT_FALSE(p.trySetGridId(1.0f));
	EXPECT_EQ(p.getGridId(), 0);
}

TEST(InputValidationTest, SearcherRejectsBadPositionsWithoutShiftingIndices)
{
	IndexedSortBasedSearcher searcher(1.0f);
	EXPECT_TRUE(searcher.add(Vector3df(0.0f, 0.0f, 0.0f)));  // 0
	EXPECT_FALSE(searcher.add(Vector3df(kNaN, 0.0f, 0.0f))); // 1 (rejected)
	EXPECT_TRUE(searcher.add(Vector3df(0.5f, 0.0f, 0.0f)));  // 2
	EXPECT_FALSE(searcher.add(Vector3df(0.0f, kInf, 0.0f))); // 3 (rejected)
	EXPECT_TRUE(searcher.add(Vector3df(0.7f, 0.0f, 0.0f)));  // 4
	EXPECT_EQ(searcher.getRejectedCount(), 2u);

	searcher.search();
	std::vector<std::pair<int, int>> pairs;
	for (const auto& p : searcher.getPairs()) pairs.emplace_back(std::min(p.first, p.second), std::max(p.first, p.second));
	std::sort(pairs.begin(), pairs.end());
	// Pairs reference the original insertion indices 0, 2, 4 -- never the rejected 1 and 3.
	EXPECT_EQ(pairs, (std::vector<std::pair<int, int>>{ {0, 2}, {0, 4}, {2, 4} }));

	searcher.clear();
	EXPECT_EQ(searcher.getRejectedCount(), 0u);
	EXPECT_TRUE(searcher.add(Vector3df(0, 0, 0)));
}

TEST(InputValidationTest, SearcherWithBadRadiusRejectsEverythingAndFindsNothing)
{
	for (const float bad : { 0.0f, -1.0f, kNaN, kInf }) {
		IndexedSortBasedSearcher searcher(bad);
		EXPECT_FALSE(searcher.add(std::vector<Vector3df>{ {0, 0, 0}, {0.1f, 0, 0} })) << bad;
		EXPECT_EQ(searcher.getRejectedCount(), 2u);
		searcher.search();
		EXPECT_TRUE(searcher.getPairs().empty());
	}
}

// ---------------------------------------------------------------- CSRNeighborList

TEST(InputValidationTest, CSRBuildWithNonFiniteRadiusGivesEmptyRows)
{
	const std::vector<Vector3df> pts{ {0, 0, 0}, {0.1f, 0, 0}, {0.2f, 0, 0} };
	CSRNeighborList list;
	for (const float bad : { kNaN, kInf, -kInf, 0.0f, -2.0f }) {
		list.build(pts, bad);
		ASSERT_EQ(list.size(), pts.size()) << bad;
		for (size_t i = 0; i < pts.size(); ++i) EXPECT_TRUE(list[i].empty());
	}
}

TEST(InputValidationTest, CSRBuildSkipsRejectedPositionsOnly)
{
	const std::vector<Vector3df> pts{ {0, 0, 0}, {kNaN, 0, 0}, {0.5f, 0, 0} };
	CSRNeighborList list;
	list.build(pts, 1.0f);
	ASSERT_EQ(list.size(), 3u);
	EXPECT_EQ(sorted(list[0]), std::vector<int>{ 2 });
	EXPECT_TRUE(list[1].empty());
	EXPECT_EQ(sorted(list[2]), std::vector<int>{ 0 });
}

TEST(InputValidationTest, CSRTryBuildPositions)
{
	CSRNeighborList list;
	const std::vector<Vector3df> good{ {0, 0, 0}, {0.5f, 0, 0}, {5, 5, 5} };
	ASSERT_TRUE(list.tryBuild(good, 1.0f));
	CSRNeighborList reference;
	reference.build(good, 1.0f);
	ASSERT_EQ(list.size(), reference.size());
	for (size_t i = 0; i < list.size(); ++i) EXPECT_EQ(sorted(list[i]), sorted(reference[i]));

	// Failure leaves the list empty rather than stale.
	EXPECT_FALSE(list.tryBuild(good, 0.0f));
	EXPECT_EQ(list.size(), 0u);
	ASSERT_TRUE(list.tryBuild(good, 1.0f));
	EXPECT_FALSE(list.tryBuild(good, kNaN));
	EXPECT_EQ(list.size(), 0u);
	ASSERT_TRUE(list.tryBuild(good, 1.0f));
	EXPECT_FALSE(list.tryBuild(std::vector<Vector3df>{ {0, 0, 0}, {kInf, 0, 0} }, 1.0f));
	EXPECT_EQ(list.size(), 0u);

	EXPECT_TRUE(list.tryBuild(std::vector<Vector3df>{}, 1.0f)) << "no particles is valid";
	EXPECT_EQ(list.size(), 0u);
}

TEST(InputValidationTest, CSRTryBuildPairsRejectsInvalidPairs)
{
	CSRNeighborList list;
	ASSERT_TRUE(list.tryBuild(4, { {0, 1}, {2, 3} }));
	ASSERT_EQ(list.size(), 4u);
	EXPECT_EQ(sorted(list[0]), std::vector<int>{ 1 });

	struct Case { const char* name; size_t count; std::vector<ParticlePair> pairs; };
	const std::vector<Case> bad{
		{ "negative first", 4, { {-1, 2} } },
		{ "negative second", 4, { {0, -3} } },
		{ "first out of range", 4, { {4, 1} } },
		{ "second out of range", 4, { {1, 99} } },
		{ "self pair", 4, { {2, 2} } },
		{ "duplicate", 4, { {0, 1}, {0, 1} } },
		{ "duplicate reversed", 4, { {0, 1}, {1, 0} } },
		{ "pair with zero particles", 0, { {0, 1} } },
	};
	for (const auto& c : bad) {
		ASSERT_TRUE(list.tryBuild(4, { {0, 1} })) << c.name;
		EXPECT_FALSE(list.tryBuild(c.count, c.pairs)) << c.name;
		EXPECT_EQ(list.size(), 0u) << c.name << ": a rejected build must not leave the previous rows behind";
	}

	// Both orientations of *different* pairs are fine.
	EXPECT_TRUE(list.tryBuild(3, { {0, 1}, {2, 1} }));
	EXPECT_EQ(sorted(list[1]), (std::vector<int>{ 0, 2 }));
	// No pairs at all, and zero particles.
	EXPECT_TRUE(list.tryBuild(5, {}));
	EXPECT_EQ(list.size(), 5u);
	EXPECT_TRUE(list.tryBuild(0, {}));
	EXPECT_EQ(list.size(), 0u);
}

TEST(InputValidationTest, CSRCheckedRowAccess)
{
	CSRNeighborList list;
	EXPECT_TRUE(list.at(0).empty()) << "never built";
	list.build(3, std::vector<ParticlePair>{ {0, 1} });
	EXPECT_EQ(sorted(list.at(0)), std::vector<int>{ 1 });
	EXPECT_TRUE(list.at(2).empty()) << "valid row without neighbors";
	EXPECT_TRUE(list.at(3).empty()) << "== size()";
	EXPECT_TRUE(list.at(1000000).empty());

	const auto view = list.at(2);
	EXPECT_EQ(view.begin(), view.end());
	EXPECT_EQ(view.size(), 0u);

	list.clear();
	EXPECT_TRUE(list.at(0).empty());
	EXPECT_EQ(list.size(), 0u);
}

TEST(InputValidationTest, EmptyNeighborIndexViewIsSafeToIterate)
{
	const NeighborIndexView empty;
	int visits = 0;
	for (const int i : empty) { (void)i; ++visits; }
	EXPECT_EQ(visits, 0);
	EXPECT_TRUE(empty.empty());
	EXPECT_EQ(empty.size(), 0u);

	const std::vector<int> none;
	const NeighborIndexView fromEmptyVector(none);
	EXPECT_TRUE(fromEmptyVector.empty());
}

// ---------------------------------------------------------------- hashes

TEST(InputValidationTest, HashesAcceptDegenerateTableSizes)
{
	for (const int tableSize : { 0, -1, -1000 }) {
		SpaceHash hash(1.0, tableSize);
		EXPECT_TRUE(hash.add(Vector3df(0, 0, 0)));
		EXPECT_TRUE(hash.add(Vector3df(0.5f, 0, 0)));
		EXPECT_EQ(sortedOf(hash.findNeighborIndices(Vector3df(0, 0, 0))), (std::vector<int>{ 0, 1 }));

		CompactSpaceHash compact(1.0, tableSize);
		EXPECT_TRUE(compact.add(Vector3df(0, 0, 0)));
		EXPECT_TRUE(compact.add(Vector3df(0.5f, 0, 0)));
		EXPECT_EQ(sortedOf(compact.findNeighborIndices(0)), std::vector<int>{ 1 });
	}
}

TEST(InputValidationTest, HashesRejectBadCellSize)
{
	for (const double bad : { 0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
		SpaceHash hash(bad, 17);
		EXPECT_FALSE(hash.add(Vector3df(0, 0, 0))) << bad;
		EXPECT_TRUE(hash.findNeighborIndices(Vector3df(0, 0, 0)).empty());

		CompactSpaceHash compact(bad, 17);
		EXPECT_FALSE(compact.add(Vector3df(0, 0, 0))) << bad;
		EXPECT_TRUE(compact.findNeighborIndices(Vector3df(0, 0, 0)).empty());
		EXPECT_TRUE(compact.findNeighborIndices(0).empty());
		EXPECT_TRUE(compact.find(0).empty());
		EXPECT_TRUE(compact.isEmpty(Vector3df(0, 0, 0)));
	}
}

TEST(InputValidationTest, HashesRejectNonFinitePositionButKeepIndices)
{
	SpaceHash hash(1.0, 31);
	EXPECT_TRUE(hash.add(Vector3df(0, 0, 0)));        // 0
	EXPECT_FALSE(hash.add(Vector3df(kNaN, 0, 0)));    // 1
	EXPECT_TRUE(hash.add(Vector3df(0.5f, 0, 0)));     // 2
	EXPECT_EQ(sortedOf(hash.findNeighborIndices(Vector3df(0, 0, 0))), (std::vector<int>{ 0, 2 }));
	EXPECT_TRUE(hash.findNeighborIndices(Vector3df(kNaN, 0, 0)).empty());
	EXPECT_TRUE(hash.findNeighborIndices(Vector3df(0, kInf, 0)).empty());

	CompactSpaceHash compact(1.0, 31);
	EXPECT_TRUE(compact.add(Vector3df(0, 0, 0)));     // 0
	EXPECT_FALSE(compact.add(Vector3df(0, kInf, 0))); // 1
	EXPECT_TRUE(compact.add(Vector3df(0.5f, 0, 0)));  // 2
	EXPECT_EQ(sortedOf(compact.findNeighborIndices(0)), std::vector<int>{ 2 });
	EXPECT_TRUE(compact.findNeighborIndices(1).empty()) << "rejected particle has no neighbors";
	EXPECT_TRUE(compact.findNeighborIndices(Vector3df(kNaN, 0, 0)).empty());
	EXPECT_TRUE(compact.find(1).empty());
	EXPECT_TRUE(compact.isEmpty(Vector3df(kNaN, 0, 0)));
	EXPECT_FALSE(compact.remove(1)) << "rejected particle belongs to no cell";
}

TEST(InputValidationTest, CompactSpaceHashIndexArgumentsAreBoundsChecked)
{
	CompactSpaceHash hash(1.0, 31);
	EXPECT_TRUE(hash.findNeighborIndices(0).empty()) << "empty hash";
	hash.add(Vector3df(0, 0, 0));
	for (const int bad : { -1, 1, 100, std::numeric_limits<int>::min(), std::numeric_limits<int>::max() }) {
		EXPECT_TRUE(hash.findNeighborIndices(bad).empty()) << bad;
		EXPECT_TRUE(hash.find(bad).empty()) << bad;
		EXPECT_FALSE(hash.remove(bad)) << bad;
	}
}

TEST(InputValidationTest, CompactSpaceHashRemoveReportsResult)
{
	CompactSpaceHash hash(1.0, 31);
	hash.add(Vector3df(0, 0, 0));    // 0
	hash.add(Vector3df(0.5f, 0, 0)); // 1
	EXPECT_TRUE(hash.remove(0));
	EXPECT_FALSE(hash.remove(0)) << "already removed";
	EXPECT_TRUE(hash.findNeighborIndices(1).empty());
	EXPECT_TRUE(hash.remove(1));
	EXPECT_TRUE(hash.isEmpty(Vector3df(0.5f, 0, 0)));
}

TEST(InputValidationTest, CompactSpaceHashIsUsableByDefaultAndAfterClear)
{
	CompactSpaceHash hash; // used to leave divideLength uninitialized and have no buckets
	EXPECT_TRUE(hash.add(Vector3df(0.2f, 0, 0)));
	EXPECT_TRUE(hash.add(Vector3df(0.4f, 0, 0)));
	EXPECT_EQ(sortedOf(hash.findNeighborIndices(0)), std::vector<int>{ 1 });

	hash.clear(); // used to drop the buckets, so the next add divided by zero
	EXPECT_TRUE(hash.isEmpty(Vector3df(0.2f, 0, 0)));
	EXPECT_TRUE(hash.add(Vector3df(0.2f, 0, 0)));
	EXPECT_TRUE(hash.add(Vector3df(0.3f, 0, 0)));
	EXPECT_EQ(sortedOf(hash.findNeighborIndices(0)), std::vector<int>{ 1 }) << "indices restart at 0 after clear()";

	hash.setup(2.0, 0);
	EXPECT_TRUE(hash.add(Vector3df(0, 0, 0)));
	EXPECT_TRUE(hash.add(Vector3df(1.5f, 0, 0)));
	EXPECT_EQ(sortedOf(hash.findNeighborIndices(0)), std::vector<int>{ 1 });
}

TEST(InputValidationTest, CompactSpaceHashCellsAliasingInMortonCodeStaySeparate)
{
	// With cell size 1, x = 0.5 and x = 2048.5 are cells 0 and 2048. They share a Morton code (10 bits per
	// axis) -- and, with one bucket, a bucket -- but are different cells and must not be merged.
	CompactSpaceHash hash(1.0, 1);
	hash.add(Vector3df(0.5f, 0.5f, 0.5f));
	hash.add(Vector3df(2048.5f, 0.5f, 0.5f));
	EXPECT_EQ(hash.toZIndex(hash.toIndex(Vector3df(0.5f, 0.5f, 0.5f))), hash.toZIndex(hash.toIndex(Vector3df(2048.5f, 0.5f, 0.5f))));
	EXPECT_EQ(sortedOf(hash.find(0)), std::vector<int>{ 0 });
	EXPECT_EQ(sortedOf(hash.find(1)), std::vector<int>{ 1 });
	EXPECT_TRUE(hash.findNeighborIndices(0).empty());
	EXPECT_FALSE(hash.isEmpty(Vector3df(2048.5f, 0.5f, 0.5f)));
}

TEST(InputValidationTest, HashesWorkWithHugeButFiniteCoordinates)
{
	SpaceHash hash(1.0, 31);
	CompactSpaceHash compact(1.0, 31);
	for (const float big : { 1.0e6f, -1.0e6f, 1.0e8f }) {
		const Vector3df p(big, big, big);
		EXPECT_TRUE(hash.add(p));
		EXPECT_TRUE(compact.add(p));
		EXPECT_EQ(hash.findNeighborIndices(p).size(), 1u);
		EXPECT_FALSE(compact.isEmpty(p));
	}
	// Beyond the cell limit (2^30 cells) the position is rejected instead of overflowing the int cast.
	EXPECT_FALSE(hash.add(Vector3df(1.0e12f, 0, 0)));
	EXPECT_FALSE(compact.add(Vector3df(0, -1.0e12f, 0)));
}

// ---------------------------------------------------------------- ZIndexedSearcher / Morton

TEST(InputValidationTest, ZIndexedSearcherRejectsOutOfRangePositions)
{
	ZIndexedSearcher searcher(1.0f, Vector3df(0, 0, 0));
	EXPECT_TRUE(searcher.add(Vector3df(0.0f, 0.0f, 0.0f)));    // 0
	EXPECT_FALSE(searcher.add(Vector3df(-0.5f, 0.0f, 0.0f)));  // 1 below minPosition
	EXPECT_FALSE(searcher.add(Vector3df(kNaN, 0.0f, 0.0f)));   // 2
	EXPECT_FALSE(searcher.add(Vector3df(2000.0f, 0.0f, 0.0f))); // 3 beyond 10-bit Morton range
	EXPECT_TRUE(searcher.add(Vector3df(0.5f, 0.0f, 0.0f)));    // 4
	searcher.sort();
	EXPECT_EQ(sortedOf(searcher.findNeighbors(Vector3df(0, 0, 0))), (std::vector<int>{ 0, 4 }));
	EXPECT_TRUE(searcher.findNeighbors(Vector3df(-1.0f, 0, 0)).empty());
	EXPECT_TRUE(searcher.findNeighbors(Vector3df(kInf, 0, 0)).empty());
	EXPECT_TRUE(searcher.findNeighbors(Vector3df(5000.0f, 0, 0)).empty());
}

TEST(InputValidationTest, ZIndexedSearcherBadRadiusFindsNothing)
{
	for (const float bad : { 0.0f, -1.0f, kNaN, kInf }) {
		ZIndexedSearcher searcher(bad, Vector3df(0, 0, 0));
		EXPECT_FALSE(searcher.add(Vector3df(0, 0, 0))) << bad;
		searcher.sort();
		EXPECT_TRUE(searcher.findNeighbors(Vector3df(0, 0, 0)).empty());
	}
	ZIndexedSearcher badOrigin(1.0f, Vector3df(kNaN, 0, 0));
	EXPECT_FALSE(badOrigin.add(Vector3df(0, 0, 0)));
}

TEST(InputValidationTest, ZIndexedSearcherFindsNeighborInUpperCornerCell)
{
	// Regression: the neighbor in cell (+1,+1,+1) was excluded by an off-by-one range end.
	ZIndexedSearcher searcher(1.0f, Vector3df(0, 0, 0));
	searcher.add(Vector3df(0.9f, 0.9f, 0.9f)); // 0, cell (0,0,0)
	searcher.add(Vector3df(1.1f, 1.1f, 1.1f)); // 1, cell (1,1,1), distance ~0.35
	searcher.add(Vector3df(0.9f, 0.9f, 0.9f)); // 2
	searcher.sort();
	EXPECT_EQ(sortedOf(searcher.findNeighbors(Vector3df(0.9f, 0.9f, 0.9f))), (std::vector<int>{ 0, 1, 2 }));
	// ...and the lower corner (-1,-1,-1) from the other side.
	EXPECT_EQ(sortedOf(searcher.findNeighbors(Vector3df(1.1f, 1.1f, 1.1f))), (std::vector<int>{ 0, 1, 2 }));
}

TEST(InputValidationTest, ZIndexedSearcherNonUnitRadiusAndOrigin)
{
	// Regression: neighbor cells used to be looked up through toIndex() as if cell numbers were world
	// positions, which is only right for radius 1 and origin 0.
	ZIndexedSearcher searcher(0.25f, Vector3df(-5.0f, 2.0f, 10.0f));
	std::vector<Vector3df> pts;
	for (int i = 0; i < 40; ++i) pts.emplace_back(-5.0f + 0.1f * i, 2.0f + 0.07f * i, 10.0f + 0.05f * i);
	for (const auto& p : pts) EXPECT_TRUE(searcher.add(p));
	searcher.sort();
	for (size_t i = 0; i < pts.size(); ++i) {
		std::vector<int> expected;
		for (size_t j = 0; j < pts.size(); ++j) {
			if (getDistanceSquared(pts[i], pts[j]) < 0.25f * 0.25f) expected.push_back(static_cast<int>(j));
		}
		EXPECT_EQ(sortedOf(searcher.findNeighbors(pts[i])), expected) << i;
	}
}

TEST(InputValidationTest, MortonEncodableRange)
{
	EXPECT_EQ(ZOrderCurve3d::kMaxAxisIndex, 1023u);
	EXPECT_TRUE(ZOrderCurve3d::isEncodable({ 0, 0, 0 }));
	EXPECT_TRUE(ZOrderCurve3d::isEncodable({ 1023, 1023, 1023 }));
	EXPECT_FALSE(ZOrderCurve3d::isEncodable({ 1024, 0, 0 }));
	EXPECT_FALSE(ZOrderCurve3d::isEncodable({ 0, 0, 0xFFFFFFFFu }));
	// Outside the range the code is not meaningful: 1024 spills past bit 29, 2048 aliases 0.
	EXPECT_GE(ZOrderCurve3d::encode({ 1024, 0, 0 }), 1u << 30);
	EXPECT_EQ(ZOrderCurve3d::encode({ 2048, 0, 0 }), ZOrderCurve3d::encode({ 0, 0, 0 }));
}
