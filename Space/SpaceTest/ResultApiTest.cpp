// Phase 3: the buffer-reusing / callback query APIs and the neighbor-search workspace must return exactly
// what the original value-returning APIs return -- same elements in the same order, not just the same set
// (SPH solvers sum over neighbor rows in order, so row order is part of the contract). Result-lifetime
// rules are exercised too: stale buffer contents, shared scratch across calls, copies, concurrent readers.
#include "gtest/gtest.h"

#include "../Space/BVH.h"
#include "../Space/CompactSpaceHash.h"
#include "../Space/IndexedSortBasedSearcher.h"
#include "../Space/KDTree.h"
#include "../Space/NeighborList.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <random>
#include <thread>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {

	std::vector<Vector3df> makePoints(const unsigned seed, const size_t count, const float extent)
	{
		std::mt19937 rng(seed);
		std::uniform_real_distribution<float> d(0.0f, extent);
		std::vector<Vector3df> pts;
		for (size_t i = 0; i < count; ++i) { const float x = d(rng), y = d(rng), z = d(rng); pts.emplace_back(x, y, z); }
		return pts;
	}

	std::vector<std::vector<int>> rowsOf(const CSRNeighborList& list)
	{
		std::vector<std::vector<int>> rows;
		for (size_t i = 0; i < list.size(); ++i) rows.emplace_back(list[i].begin(), list[i].end());
		return rows; // deliberately NOT sorted: the order is under test
	}

	struct BvhFixture
	{
		std::vector<std::unique_ptr<BVHObject>> owned;
		std::vector<BVHObject*> ptrs;
		explicit BvhFixture(const unsigned seed, const size_t n)
		{
			const auto pts = makePoints(seed, n, 10.0f);
			for (size_t i = 0; i < n; ++i) {
				owned.push_back(std::make_unique<BVHObject>(static_cast<int>(i), Box3df(pts[i], pts[i] + Vector3df(0.5f, 0.4f, 0.3f))));
				ptrs.push_back(owned.back().get());
			}
		}
	};
}

// ---------------------------------------------------------------- CSR workspace

TEST(ResultApiTest, CSRRebuildOrderIsIdenticalToAFreshListAndToThePreviousBuild)
{
	const auto pts = makePoints(1u, 4000, 12.0f);
	CSRNeighborList reused;
	reused.build(pts, 1.0f);
	const auto firstRows = rowsOf(reused);

	CSRNeighborList fresh;
	fresh.build(pts, 1.0f);
	EXPECT_EQ(rowsOf(fresh), firstRows);

	for (int i = 0; i < 3; ++i) {
		reused.build(pts, 1.0f);
		EXPECT_EQ(rowsOf(reused), firstRows) << "rebuild " << i << " must not change the row order";
	}
}

TEST(ResultApiTest, CSRWorkspaceSurvivesChangingProblemSizeAndRadius)
{
	CSRNeighborList reused;
	const struct { unsigned seed; size_t n; float extent; float r; } cases[] = {
		{ 1, 3000, 10.0f, 1.0f }, { 2, 200, 4.0f, 0.5f }, { 3, 5000, 10.0f, 1.5f }, { 4, 0, 1.0f, 1.0f },
		{ 5, 1, 1.0f, 1.0f }, { 6, 2500, 30.0f, 2.0f }, { 7, 100, 3.0f, 1.0f },
	};
	for (const auto& c : cases) {
		const auto pts = makePoints(c.seed, c.n, c.extent);
		reused.build(pts, c.r);
		CSRNeighborList fresh;
		fresh.build(pts, c.r);
		ASSERT_EQ(reused.size(), pts.size());
		EXPECT_EQ(rowsOf(reused), rowsOf(fresh)) << "n=" << c.n << " r=" << c.r;
	}
}

TEST(ResultApiTest, CSRCopyIsIndependentAndEqual)
{
	const auto pts = makePoints(1u, 1500, 8.0f);
	CSRNeighborList a;
	a.build(pts, 1.0f);

	CSRNeighborList copy(a);
	EXPECT_EQ(rowsOf(copy), rowsOf(a));

	// Rebuilding the original must not disturb the copy (no shared storage).
	const auto before = rowsOf(copy);
	a.build(makePoints(2u, 700, 8.0f), 1.0f);
	EXPECT_EQ(rowsOf(copy), before);

	// A copy can be rebuilt on its own, and assignment works.
	copy.build(pts, 1.0f);
	CSRNeighborList assigned;
	assigned = copy;
	EXPECT_EQ(rowsOf(assigned), rowsOf(copy));
	CSRNeighborList moved(std::move(assigned));
	EXPECT_EQ(rowsOf(moved), rowsOf(copy));
	moved.build(pts, 1.0f);
	EXPECT_EQ(rowsOf(moved), rowsOf(copy));
}

TEST(ResultApiTest, CSRViewsAreInvalidatedByRebuildButAnEarlierCopyKeepsItsRows)
{
	const auto pts = makePoints(1u, 500, 5.0f);
	CSRNeighborList list;
	list.build(pts, 1.0f);
	const std::vector<int> row0(list[0].begin(), list[0].end());
	CSRNeighborList snapshot(list);

	list.build(makePoints(2u, 500, 5.0f), 1.0f); // documented: views taken before this are now invalid
	const std::vector<int> snapRow0(snapshot[0].begin(), snapshot[0].end());
	EXPECT_EQ(snapRow0, row0) << "a copied list owns its rows";
}

TEST(ResultApiTest, SearcherWithExternalWorkspaceMatchesOwnedStorage)
{
	NeighborSearchWorkspace ws;
	for (const size_t n : { size_t(2000), size_t(300), size_t(2500), size_t(0), size_t(50) }) {
		const auto pts = makePoints(static_cast<unsigned>(n) + 9u, n, 9.0f);

		IndexedSortBasedSearcher owned(1.0f);
		owned.add(pts);
		owned.search();

		IndexedSortBasedSearcher borrowed(1.0f, ws); // starts empty even though ws holds the previous run
		borrowed.add(pts);
		borrowed.search();
		EXPECT_EQ(borrowed.getPairs(), owned.getPairs()) << "n=" << n << " (pair order included)";
		EXPECT_EQ(&borrowed.getPairs(), &ws.pairs) << "getPairs() refers to the workspace";
	}
}

TEST(ResultApiTest, SearcherRepeatedSearchOnOneWorkspaceKeepsReplacingPairs)
{
	NeighborSearchWorkspace ws;
	const auto pts = makePoints(3u, 1200, 7.0f);
	IndexedSortBasedSearcher searcher(1.0f, ws);
	searcher.add(pts);
	searcher.search();
	const auto first = searcher.getPairs();
	searcher.search();
	EXPECT_EQ(searcher.getPairs(), first);
	searcher.clear();
	EXPECT_TRUE(searcher.getPairs().empty());
}

// ---------------------------------------------------------------- KDTree

TEST(ResultApiTest, KDTreeBufferAndCallbackOverloadsMatchValueOverloads)
{
	const auto pts = makePoints(1u, 3000, 10.0f);
	KDTree tree;
	tree.build(pts);

	std::vector<int> out = { -7, -7, -7 }; // stale content must be discarded
	KDTree::KNearestScratch scratch;
	scratch.heap.push_back({ 1.0f, 99 });  // stale scratch must be discarded too
	for (const auto& q : makePoints(2u, 80, 10.0f)) {
		for (const float r : { 0.0f, 0.4f, 1.5f }) {
			const auto expected = tree.findWithinRadius(q, r);
			tree.findWithinRadius(q, r, out);
			EXPECT_EQ(out, expected);
			std::vector<int> visited;
			tree.forEachWithinRadius(q, r, [&](const int i) { visited.push_back(i); });
			EXPECT_EQ(visited, expected) << "callback order == vector order";
		}
		for (const size_t k : { size_t(1), size_t(7), size_t(64), size_t(5000) }) {
			const auto expected = tree.findKNearestIndices(q, k);
			tree.findKNearestIndices(q, k, out, scratch);
			EXPECT_EQ(out, expected) << "k=" << k;
		}
	}

	// Degenerate inputs clear the buffer.
	out = { 1, 2, 3 };
	tree.findWithinRadius(Vector3df(0, 0, 0), -1.0f, out);
	EXPECT_TRUE(out.empty());
	out = { 1, 2, 3 };
	tree.findKNearestIndices(Vector3df(0, 0, 0), 0, out, scratch);
	EXPECT_TRUE(out.empty());
	KDTree empty;
	out = { 1, 2, 3 };
	empty.findWithinRadius(Vector3df(0, 0, 0), 5.0f, out);
	EXPECT_TRUE(out.empty());
	empty.findKNearestIndices(Vector3df(0, 0, 0), 3, out, scratch);
	EXPECT_TRUE(out.empty());
	int visits = 0;
	empty.forEachWithinRadius(Vector3df(0, 0, 0), 5.0f, [&](int) { ++visits; });
	tree.forEachWithinRadius(Vector3df(0, 0, 0), std::numeric_limits<float>::quiet_NaN(), [&](int) { ++visits; });
	EXPECT_EQ(visits, 0);
}

TEST(ResultApiTest, KDTreeIndicesRefreshAfterRebuildIntoTheSameBuffer)
{
	KDTree tree;
	std::vector<int> out;
	for (unsigned seed = 1; seed <= 4; ++seed) {
		const auto pts = makePoints(seed, 400 * seed, 6.0f);
		tree.build(pts);
		const Vector3df q(3, 3, 3);
		tree.findWithinRadius(q, 1.0f, out);
		EXPECT_EQ(out, tree.findWithinRadius(q, 1.0f));
		for (const int i : out) {
			ASSERT_LT(static_cast<size_t>(i), pts.size());
			EXPECT_LE(getDistanceSquared(pts[i], q), 1.0f);
		}
	}
}

// ---------------------------------------------------------------- CompactSpaceHash

TEST(ResultApiTest, CompactSpaceHashBufferAndCallbackOverloadsMatchValueOverloads)
{
	const auto pts = makePoints(1u, 3000, 10.0f);
	CompactSpaceHash hash(1.0, 4001);
	for (const auto& p : pts) hash.add(p);

	std::vector<int> out = { -1, -1 };
	for (int i = 0; i < 3000; i += 7) {
		const auto expected = hash.findNeighborIndices(i);
		hash.findNeighborIndices(i, out);
		EXPECT_EQ(out, expected);
		std::vector<int> visited;
		hash.forEachNeighbor(i, [&](const int j) { visited.push_back(j); });
		EXPECT_EQ(visited, expected);
	}
	for (const auto& q : makePoints(2u, 50, 10.0f)) {
		const auto expected = hash.findNeighborIndices(q);
		hash.findNeighborIndices(q, out);
		EXPECT_EQ(out, expected);
		std::vector<int> visited;
		hash.forEachNeighbor(q, [&](const int j) { visited.push_back(j); });
		EXPECT_EQ(visited, expected);
	}

	// Invalid input clears the buffer / visits nothing.
	out = { 1, 2, 3 };
	hash.findNeighborIndices(-1, out);
	EXPECT_TRUE(out.empty());
	out = { 1, 2, 3 };
	hash.findNeighborIndices(Vector3df(std::numeric_limits<float>::quiet_NaN(), 0, 0), out);
	EXPECT_TRUE(out.empty());
	int visits = 0;
	hash.forEachNeighbor(5000, [&](int) { ++visits; });
	hash.forEachNeighbor(Vector3df(0, std::numeric_limits<float>::infinity(), 0), [&](int) { ++visits; });
	EXPECT_EQ(visits, 0);
}

TEST(ResultApiTest, CompactSpaceHashQueriesAreConstCallable)
{
	CompactSpaceHash hash(1.0, 31);
	hash.add(Vector3df(0, 0, 0));
	hash.add(Vector3df(0.5f, 0, 0));
	const CompactSpaceHash& view = hash;
	EXPECT_EQ(view.findNeighborIndices(0), std::vector<int>{ 1 });
	EXPECT_EQ(view.findNeighborIndices(Vector3df(0, 0, 0)).size(), 2u);
}

// ---------------------------------------------------------------- BVH

TEST(ResultApiTest, BVHBufferAndCallbackOverloadsMatchValueOverloads)
{
	BvhFixture f(1u, 1500);
	BVH bvh(f.ptrs, 4);
	std::mt19937 rng(5u);
	std::uniform_real_distribution<float> pos(0.0f, 10.0f), comp(0.2f, 1.0f);

	std::vector<BVHObject*> out = { nullptr };
	for (int q = 0; q < 60; ++q) {
		const Vector3df lo(pos(rng), pos(rng), pos(rng));
		const Box3df box(lo, lo + Vector3df(2, 2, 2));
		const auto expected = bvh.queryOverlaps(box);
		bvh.queryOverlaps(box, out);
		EXPECT_EQ(out, expected);
		std::vector<BVHObject*> visited;
		bvh.forEachOverlap(box, [&](BVHObject* o) { visited.push_back(o); });
		EXPECT_EQ(visited, expected);

		const Vector3df origin(pos(rng) - 5.0f, pos(rng), pos(rng));
		const Vector3df dir(comp(rng), comp(rng) - 0.6f, comp(rng) - 0.6f);
		const auto rayExpected = bvh.queryRay(origin, dir);
		bvh.queryRay(origin, dir, out);
		EXPECT_EQ(out, rayExpected);
		std::vector<BVHObject*> rayVisited;
		bvh.forEachRayCandidate(origin, dir, 1e-4f, 1e38f, [&](BVHObject* o) { rayVisited.push_back(o); });
		EXPECT_EQ(rayVisited, rayExpected);
		// Explicit t range too.
		EXPECT_EQ(bvh.queryRay(origin, dir, 0.0f, 3.0f).size() <= rayExpected.size(), true);
		std::vector<BVHObject*> limited;
		bvh.queryRay(origin, dir, limited, 0.0f, 3.0f);
		EXPECT_EQ(limited, bvh.queryRay(origin, dir, 0.0f, 3.0f));
	}

	std::vector<std::pair<int, int>> pairs = { {9, 9} }, stack = { {1, 1} };
	bvh.findAllPairs(pairs, stack);
	EXPECT_EQ(pairs, bvh.findAllPairs());
	// The same buffers can be reused after the objects move and the tree is refitted.
	for (auto* o : f.ptrs) o->box = Box3df(o->box.getMin() + Vector3df(1, 0, 0), o->box.getMax() + Vector3df(1, 0, 0));
	bvh.refit();
	bvh.findAllPairs(pairs, stack);
	EXPECT_EQ(pairs, bvh.findAllPairs());
}

TEST(ResultApiTest, BVHEmptyTreeClearsBuffers)
{
	BVH bvh(std::vector<BVHObject*>{}, 4);
	std::vector<BVHObject*> out = { nullptr, nullptr };
	bvh.queryOverlaps(Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1)), out);
	EXPECT_TRUE(out.empty());
	out = { nullptr };
	bvh.queryRay(Vector3df(0, 0, 0), Vector3df(1, 1, 1), out);
	EXPECT_TRUE(out.empty());
	std::vector<std::pair<int, int>> pairs = { {1, 2} }, stack = { {3, 4} };
	bvh.findAllPairs(pairs, stack);
	EXPECT_TRUE(pairs.empty());
}

TEST(ResultApiTest, BVHTraversalStackHoldsForDegenerateInputs)
{
	// All boxes identical: every object overlaps every query, and the median split still halves the set,
	// so the fixed-size traversal stack must not overflow.
	std::vector<std::unique_ptr<BVHObject>> owned;
	std::vector<BVHObject*> ptrs;
	for (int i = 0; i < 20000; ++i) {
		owned.push_back(std::make_unique<BVHObject>(i, Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1))));
		ptrs.push_back(owned.back().get());
	}
	BVH bvh(ptrs, 1);
	std::vector<BVHObject*> out;
	bvh.queryOverlaps(Box3df(Vector3df(0.5f, 0.5f, 0.5f), Vector3df(0.6f, 0.6f, 0.6f)), out);
	EXPECT_EQ(out.size(), ptrs.size());
	bvh.queryRay(Vector3df(-1, 0.5f, 0.5f), Vector3df(1, 0.1f, 0.1f), out);
	EXPECT_EQ(out.size(), ptrs.size());
}

// ---------------------------------------------------------------- concurrent readers

TEST(ResultApiTest, ConstQueriesWithPerThreadBuffersAgreeWithSerialResults)
{
	const auto pts = makePoints(1u, 4000, 10.0f);
	KDTree tree;
	tree.build(pts);
	CompactSpaceHash hash(1.0, 8009);
	for (const auto& p : pts) hash.add(p);
	BvhFixture f(2u, 2000);
	BVH bvh(f.ptrs, 4);

	const auto queries = makePoints(3u, 400, 10.0f);
	std::vector<std::vector<int>> kdExpected, hashExpected;
	std::vector<std::vector<BVHObject*>> bvhExpected;
	for (const auto& q : queries) {
		kdExpected.push_back(tree.findWithinRadius(q, 0.8f));
		hashExpected.push_back(hash.findNeighborIndices(q));
		bvhExpected.push_back(bvh.queryOverlaps(Box3df(q, q + Vector3df(1, 1, 1))));
	}

	constexpr int kThreads = 4;
	std::vector<int> failures(kThreads, 0);
	std::vector<std::thread> threads;
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&, t] {
			std::vector<int> kdOut, hashOut;
			std::vector<BVHObject*> bvhOut;
			KDTree::KNearestScratch scratch;
			for (size_t i = static_cast<size_t>(t); i < queries.size(); i += kThreads) {
				tree.findWithinRadius(queries[i], 0.8f, kdOut);
				hash.findNeighborIndices(queries[i], hashOut);
				bvh.queryOverlaps(Box3df(queries[i], queries[i] + Vector3df(1, 1, 1)), bvhOut);
				tree.findKNearestIndices(queries[i], 8, kdOut, scratch);
				tree.findWithinRadius(queries[i], 0.8f, kdOut);
				if (kdOut != kdExpected[i] || hashOut != hashExpected[i] || bvhOut != bvhExpected[i]) ++failures[t];
			}
		});
	}
	for (auto& th : threads) th.join();
	for (int t = 0; t < kThreads; ++t) EXPECT_EQ(failures[t], 0) << "thread " << t;
}

