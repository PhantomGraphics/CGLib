// Phase 4: build / update / ownership contracts of KDTree, Octree and BVH (docs/spec/space_api_contracts.md).
// Covers staging vs built state, non-finite input, non-owning items, duplicates, out-of-domain items,
// id uniqueness, and repeated move -> refit / rebuild cycles checked against brute force.
#include "gtest/gtest.h"

#include "../Core/BVH.h"
#include "../Core/KDTree.h"
#include "../Core/Octree.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {
	constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
	constexpr float kInf = std::numeric_limits<float>::infinity();

	std::vector<Vector3df> makePoints(const unsigned seed, const size_t n, const float extent)
	{
		std::mt19937 rng(seed);
		std::uniform_real_distribution<float> d(0.0f, extent);
		std::vector<Vector3df> pts;
		for (size_t i = 0; i < n; ++i) { const float x = d(rng), y = d(rng), z = d(rng); pts.emplace_back(x, y, z); }
		return pts;
	}

	std::vector<int> sorted(std::vector<int> v) { std::sort(v.begin(), v.end()); return v; }

	class TestItem : public ITreeItem
	{
	public:
		explicit TestItem(const Box3df& b) : box(b) {}
		Box3df getBox() override { return box; }
		Box3df box;
	};

	Box3df cube(const Vector3df& lo, const float size) { return Box3df(lo, lo + Vector3df(size, size, size)); }
}

// ---------------------------------------------------------------- KDTree

TEST(TreeContractTest, KDTreeStagedPointsAreInvisibleUntilBuild)
{
	KDTree tree;
	tree.addPoint(Vector3df(0, 0, 0));
	tree.addPoint(Vector3df(1, 0, 0));
	tree.addPoint(Vector3df(2, 0, 0));
	EXPECT_TRUE(tree.isDirty()) << "nothing built yet";
	tree.build();
	EXPECT_FALSE(tree.isDirty());
	EXPECT_EQ(tree.size(), 3u);

	EXPECT_TRUE(tree.addPoint(Vector3df(10, 0, 0)));
	EXPECT_TRUE(tree.isDirty());
	EXPECT_EQ(tree.pointCount(), 4u);
	EXPECT_EQ(tree.size(), 3u);

	// The staged point is not reachable; the query answers from the last build, safely.
	EXPECT_EQ(tree.findNearestIndex(Vector3df(10, 0, 0)), 2);
	EXPECT_EQ(sorted(tree.findWithinRadius(Vector3df(10, 0, 0), 100.0f)), (std::vector<int>{ 0, 1, 2 }));
	EXPECT_EQ(sorted(tree.findKNearestIndices(Vector3df(10, 0, 0), 50)), (std::vector<int>{ 0, 1, 2 }));

	tree.build();
	EXPECT_FALSE(tree.isDirty());
	EXPECT_EQ(tree.size(), 4u);
	EXPECT_EQ(tree.findNearestIndex(Vector3df(10, 0, 0)), 3);
}

TEST(TreeContractTest, KDTreeBuildTwiceAndClearAreConsistent)
{
	KDTree tree;
	const auto pts = makePoints(1u, 300, 5.0f);
	for (const auto& p : pts) tree.addPoint(p);
	tree.build();
	const auto first = tree.findWithinRadius(Vector3df(2, 2, 2), 1.0f);
	tree.build(); // rebuild from the same staged points
	EXPECT_EQ(tree.findWithinRadius(Vector3df(2, 2, 2), 1.0f), first) << "build() is deterministic";

	tree.clear();
	EXPECT_EQ(tree.size(), 0u);
	EXPECT_EQ(tree.pointCount(), 0u);
	EXPECT_FALSE(tree.isDirty());
	EXPECT_EQ(tree.rejectedCount(), 0u);
	EXPECT_EQ(tree.findNearestIndex(Vector3df(0, 0, 0)), -1);

	// Reusing a cleared tree starts indices from 0 again.
	tree.addPoint(Vector3df(7, 7, 7));
	tree.build();
	EXPECT_EQ(tree.findNearestIndex(Vector3df(0, 0, 0)), 0);
}

TEST(TreeContractTest, KDTreeSkipsNonFinitePointsButKeepsTheirIndices)
{
	KDTree tree;
	EXPECT_TRUE(tree.addPoint(Vector3df(0, 0, 0)));    // 0
	EXPECT_FALSE(tree.addPoint(Vector3df(kNaN, 0, 0))); // 1
	EXPECT_TRUE(tree.addPoint(Vector3df(1, 0, 0)));    // 2
	EXPECT_FALSE(tree.addPoint(Vector3df(0, kInf, 0))); // 3
	EXPECT_TRUE(tree.addPoint(Vector3df(2, 0, 0)));    // 4
	tree.build();
	EXPECT_EQ(tree.size(), 3u);
	EXPECT_EQ(tree.pointCount(), 5u);
	EXPECT_EQ(tree.rejectedCount(), 2u);

	EXPECT_EQ(sorted(tree.findWithinRadius(Vector3df(0, 0, 0), 5.0f)), (std::vector<int>{ 0, 2, 4 }));
	EXPECT_EQ(tree.findNearestIndex(Vector3df(1.9f, 0, 0)), 4);
	EXPECT_EQ(sorted(tree.findKNearestIndices(Vector3df(0, 0, 0), 10)), (std::vector<int>{ 0, 2, 4 }));

	KDTree allBad;
	allBad.build(std::vector<Vector3df>{ Vector3df(kNaN, kNaN, kNaN), Vector3df(kInf, 0, 0) });
	EXPECT_EQ(allBad.size(), 0u);
	EXPECT_EQ(allBad.rejectedCount(), 2u);
	EXPECT_EQ(allBad.findNearestIndex(Vector3df(0, 0, 0)), -1);
	EXPECT_TRUE(allBad.findWithinRadius(Vector3df(0, 0, 0), 1e9f).empty());
}

TEST(TreeContractTest, KDTreeNonFiniteQueriesReturnNothing)
{
	KDTree tree;
	tree.build(makePoints(2u, 100, 3.0f));
	for (const auto& q : { Vector3df(kNaN, 0, 0), Vector3df(0, kInf, 0), Vector3df(0, 0, -kInf) }) {
		EXPECT_EQ(tree.findNearestIndex(q), -1);
		EXPECT_EQ(tree.findNearest(q), nullptr);
		EXPECT_TRUE(tree.findWithinRadius(q, 1.0f).empty());
		EXPECT_TRUE(tree.findKNearestIndices(q, 5).empty());
		int visits = 0;
		tree.forEachWithinRadius(q, 1.0f, [&](int) { ++visits; });
		EXPECT_EQ(visits, 0);
	}
}

TEST(TreeContractTest, KDTreeFindNearestPointsIntoOwnedStorage)
{
	KDTree tree;
	tree.build(std::vector<Vector3df>{ Vector3df(1, 2, 3), Vector3df(9, 9, 9) });
	const Vector3df* p = tree.findNearest(Vector3df(1, 2, 2));
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(*p, Vector3df(1, 2, 3)) << "valid until the next build()/clear()/addPoint()";
}

// ---------------------------------------------------------------- Octree

TEST(TreeContractTest, OctreeNeverSubdividesWhenEveryItemFitsTheRoot)
{
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	Octree tree(domain);
	std::vector<std::unique_ptr<TestItem>> items;
	for (const auto& p : makePoints(1u, 500, 9.0f)) {
		items.push_back(std::make_unique<TestItem>(cube(p, 0.5f)));
		EXPECT_TRUE(tree.add(items.back().get()));
	}
	EXPECT_EQ(tree.countItems(), 500u);
	EXPECT_EQ(tree.getItemsCount(), 500u) << "all items sit in the root";
	for (const auto& child : tree.getChildren()) EXPECT_EQ(child, nullptr);
}

TEST(TreeContractTest, OctreeIsAtMostOneLevelDeep)
{
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	Octree tree(domain);
	std::vector<std::unique_ptr<TestItem>> items;
	// One item that straddles the root boundary forces the split.
	items.push_back(std::make_unique<TestItem>(Box3df(Vector3df(8, 8, 8), Vector3df(12, 12, 12))));
	EXPECT_TRUE(tree.add(items.back().get()));
	for (const auto& p : makePoints(2u, 400, 9.0f)) {
		items.push_back(std::make_unique<TestItem>(cube(p, 0.05f)));
		EXPECT_TRUE(tree.add(items.back().get()));
	}
	EXPECT_EQ(tree.countItems(), 401u);
	int created = 0;
	for (const auto& child : tree.getChildren()) {
		if (child == nullptr) continue;
		++created;
		for (const auto& grandChild : child->getChildren()) {
			EXPECT_EQ(grandChild, nullptr) << "no octant is ever refined further";
		}
	}
	EXPECT_EQ(created, 8);
}

TEST(TreeContractTest, OctreeAddReportsWhatHappened)
{
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	Octree tree(domain);
	TestItem inside(cube(Vector3df(1, 1, 1), 1.0f));
	TestItem outside(cube(Vector3df(20, 20, 20), 1.0f));
	TestItem nanBox(Box3df(Vector3df(kNaN, 0, 0), Vector3df(1, 1, 1)));
	TestItem infBox(Box3df(Vector3df(0, 0, 0), Vector3df(kInf, 1, 1)));
	TestItem partlyOutside(Box3df(Vector3df(9, 9, 9), Vector3df(11, 11, 11)));

	EXPECT_TRUE(tree.add(&inside));
	EXPECT_FALSE(tree.add(&outside));
	EXPECT_FALSE(tree.add(&nanBox));
	EXPECT_FALSE(tree.add(&infBox));
	EXPECT_FALSE(tree.add(nullptr));
	EXPECT_TRUE(tree.add(&partlyOutside)) << "intersecting the root is enough";
	EXPECT_EQ(tree.countItems(), 2u);
}

TEST(TreeContractTest, OctreeStoresDuplicatePointersTwice)
{
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	Octree tree(domain);
	TestItem item(cube(Vector3df(2, 2, 2), 1.0f));
	EXPECT_TRUE(tree.add(&item));
	EXPECT_TRUE(tree.add(&item));
	EXPECT_EQ(tree.countItems(), 2u);
	const auto hits = tree.findItems(Box3df(Vector3df(0, 0, 0), Vector3df(5, 5, 5)));
	EXPECT_EQ(hits.size(), 2u) << "no de-duplication: the caller must not add one item twice";
	EXPECT_EQ(std::count(hits.begin(), hits.end(), &item), 2);
}

TEST(TreeContractTest, OctreeSphereQueryRejectsBadRadiusAndPosition)
{
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	Octree tree(domain);
	TestItem item(cube(Vector3df(2, 2, 2), 1.0f));
	tree.add(&item);
	EXPECT_EQ(tree.findItems(Vector3df(2.5f, 2.5f, 2.5f), 0.0f).size(), 1u) << "zero radius inside the box hits";
	EXPECT_TRUE(tree.findItems(Vector3df(2.5f, 2.5f, 2.5f), -3.0f).empty()) << "was treated as radius 3";
	EXPECT_TRUE(tree.findItems(Vector3df(2.5f, 2.5f, 2.5f), kNaN).empty());
	EXPECT_TRUE(tree.findItems(Vector3df(kNaN, 2.5f, 2.5f), 5.0f).empty());
	EXPECT_EQ(tree.findItems(Vector3df(2.5f, 2.5f, 2.5f), kInf).size(), 1u);
}

TEST(TreeContractTest, OctreeItemsAreBorrowedNotCopied)
{
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	Octree tree(domain);
	TestItem item(cube(Vector3df(2, 2, 2), 1.0f));
	tree.add(&item);
	const auto hits = tree.findItems(Box3df(Vector3df(0, 0, 0), Vector3df(5, 5, 5)));
	ASSERT_EQ(hits.size(), 1u);
	EXPECT_EQ(hits.front(), &item) << "the tree hands back the caller's own pointer";
}

// ---------------------------------------------------------------- BVH

namespace {
	struct World
	{
		std::vector<std::unique_ptr<BVHObject>> owned;
		std::vector<BVHObject*> ptrs;
		explicit World(const unsigned seed, const size_t n)
		{
			for (const auto& p : makePoints(seed, n, 10.0f)) add(p);
		}
		void add(const Vector3df& p)
		{
			owned.push_back(std::make_unique<BVHObject>(static_cast<int>(owned.size()), cube(p, 0.4f)));
			ptrs.push_back(owned.back().get());
		}
		void scramble(const unsigned seed)
		{
			std::mt19937 rng(seed);
			std::uniform_real_distribution<float> d(0.0f, 10.0f);
			for (auto* o : ptrs) { const float x = d(rng), y = d(rng), z = d(rng); o->box = cube(Vector3df(x, y, z), 0.4f); }
		}
		void nudge(const unsigned seed, const float amount)
		{
			std::mt19937 rng(seed);
			std::uniform_real_distribution<float> d(-amount, amount);
			for (auto* o : ptrs) {
				const Vector3df t(d(rng), d(rng), d(rng));
				o->box = Box3df(o->box.getMin() + t, o->box.getMax() + t);
			}
		}
	};

	std::set<std::pair<int, int>> brutePairs(const World& w)
	{
		std::set<std::pair<int, int>> out;
		for (size_t i = 0; i < w.ptrs.size(); ++i)
			for (size_t j = i + 1; j < w.ptrs.size(); ++j)
				if (w.ptrs[i]->box.intersects(w.ptrs[j]->box)) out.insert({ std::min(w.ptrs[i]->id, w.ptrs[j]->id), std::max(w.ptrs[i]->id, w.ptrs[j]->id) });
		return out;
	}

	void expectMatchesBrute(const World& w, const BVH& bvh, const char* when)
	{
		const auto pairs = bvh.findAllPairs();
		EXPECT_EQ((std::set<std::pair<int, int>>(pairs.begin(), pairs.end())), brutePairs(w)) << when;
		EXPECT_EQ(pairs.size(), brutePairs(w).size()) << when << ": no duplicate pairs";
		const Box3df q(Vector3df(3, 3, 3), Vector3df(6, 6, 6));
		std::vector<int> got, expected;
		for (auto* o : bvh.queryOverlaps(q)) got.push_back(o->id);
		for (auto* o : w.ptrs) if (o->box.intersects(q)) expected.push_back(o->id);
		EXPECT_EQ(sorted(got), sorted(expected)) << when;
	}
}

TEST(TreeContractTest, BVHSkipsNullObjects)
{
	World w(1u, 50);
	auto input = w.ptrs;
	input.insert(input.begin() + 10, nullptr);
	input.push_back(nullptr);
	BVH bvh(input, 4);
	EXPECT_EQ(bvh.objectCount(), 50u);
	EXPECT_EQ(bvh.nullObjectCount(), 2u);
	expectMatchesBrute(w, bvh, "after skipping nulls");

	BVH onlyNull(std::vector<BVHObject*>{ nullptr, nullptr }, 4);
	EXPECT_EQ(onlyNull.objectCount(), 0u);
	EXPECT_TRUE(onlyNull.queryOverlaps(Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1))).empty());
	EXPECT_TRUE(onlyNull.findAllPairs().empty());
}

TEST(TreeContractTest, BVHDetectsDuplicateIdsAndDropsTheirPairs)
{
	World w(2u, 20);
	BVH unique(w.ptrs, 2);
	EXPECT_TRUE(unique.hasUniqueIds());

	// Two overlapping objects with the same id: findAllPairs() needs idA < idB, so they never pair.
	BVHObject a(7, cube(Vector3df(0, 0, 0), 1.0f));
	BVHObject b(7, cube(Vector3df(0.5f, 0, 0), 1.0f));
	BVHObject c(8, cube(Vector3df(0.25f, 0, 0), 1.0f));
	BVH dup(std::vector<BVHObject*>{ &a, &b, &c }, 1);
	EXPECT_FALSE(dup.hasUniqueIds());
	const auto pairs = dup.findAllPairs();
	const std::set<std::pair<int, int>> got(pairs.begin(), pairs.end());
	EXPECT_EQ(got, (std::set<std::pair<int, int>>{ {7, 8} })) << "the same-id pair is missing; (7,8) is reported once per pair of objects";
	EXPECT_EQ(dup.queryOverlaps(Box3df(Vector3df(0, 0, 0), Vector3df(2, 2, 2))).size(), 3u) << "queries ignore ids";
}

TEST(TreeContractTest, BVHRepeatedMoveRefitCyclesStayCorrect)
{
	World w(3u, 300);
	BVH bvh(w.ptrs, 4);
	EXPECT_FLOAT_EQ(bvh.refitDegradation(), 1.0f);
	expectMatchesBrute(w, bvh, "initial");
	for (unsigned round = 0; round < 6; ++round) {
		w.nudge(100u + round, 0.7f);
		bvh.refit();
		expectMatchesBrute(w, bvh, "after nudge + refit");
	}
}

TEST(TreeContractTest, BVHRebuildAfterLargeMovesRestoresQuality)
{
	World w(4u, 400);
	BVH bvh(w.ptrs, 4);
	EXPECT_FLOAT_EQ(bvh.refitDegradation(), 1.0f);

	w.scramble(77u); // every object jumps to an unrelated place
	bvh.refit();
	expectMatchesBrute(w, bvh, "refit after scramble (loose but correct)");
	const float degraded = bvh.refitDegradation();
	EXPECT_GT(degraded, 2.0f) << "boxes got much looser; rebuild() is now worthwhile";

	bvh.rebuild();
	EXPECT_FLOAT_EQ(bvh.refitDegradation(), 1.0f) << "the baseline is the rebuilt tree";
	expectMatchesBrute(w, bvh, "after rebuild");
	EXPECT_EQ(bvh.objectCount(), 400u);

	// Moving a little and refitting from the fresh baseline barely degrades.
	w.nudge(5u, 0.05f);
	bvh.refit();
	EXPECT_LT(bvh.refitDegradation(), degraded);
	expectMatchesBrute(w, bvh, "small move after rebuild");
}

TEST(TreeContractTest, BVHRebuildOfEmptyAndSingleTreesIsHarmless)
{
	BVH empty(std::vector<BVHObject*>{}, 4);
	empty.rebuild();
	empty.refit();
	EXPECT_EQ(empty.nodeCount(), 0);
	EXPECT_FLOAT_EQ(empty.refitDegradation(), 1.0f);
	EXPECT_TRUE(empty.hasUniqueIds());

	BVHObject one(3, cube(Vector3df(0, 0, 0), 1.0f));
	BVH bvh(std::vector<BVHObject*>{ &one }, 4);
	one.box = cube(Vector3df(5, 5, 5), 1.0f);
	bvh.rebuild();
	EXPECT_EQ(bvh.queryOverlaps(cube(Vector3df(5, 5, 5), 0.5f)).size(), 1u);
	EXPECT_TRUE(bvh.queryOverlaps(cube(Vector3df(0, 0, 0), 0.5f)).empty());
}

TEST(TreeContractTest, BVHLeafMaxIsClampedAndDoesNotChangeResults)
{
	World w(5u, 200);
	std::set<std::pair<int, int>> reference;
	for (const int leafMax : { -5, 0, 1, 2, 8, 100000 }) {
		BVH bvh(w.ptrs, leafMax);
		const auto pairs = bvh.findAllPairs();
		const std::set<std::pair<int, int>> got(pairs.begin(), pairs.end());
		if (reference.empty()) reference = got;
		EXPECT_EQ(got, reference) << "leafMax=" << leafMax;
		EXPECT_EQ(got, brutePairs(w));
	}
}
