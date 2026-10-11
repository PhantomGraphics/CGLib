// Differential tests: every spatial index is compared against a brute-force reference on fixed-seed
// data. Result sets are compared as normalized (sorted) sets, so a missed *or* duplicated id is caught
// even when the count happens to match. Contracts under test: docs/spec/space_api_contracts.md.
#include "gtest/gtest.h"

#include "../Space/BVH.h"
#include "../Space/CompactSpaceHash.h"
#include "../Space/IndexedSortBasedSearcher.h"
#include "../Space/KDTree.h"
#include "../Space/NeighborList.h"
#include "../Space/Octree.h"
#include "../Space/SpaceHash.h"
#include "../Space/ZIndexedSearcher.h"

#include <algorithm>
#include <memory>
#include <random>
#include <set>
#include <utility>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {

	using Points = std::vector<Vector3df>;
	using IdSet = std::vector<int>;

	// All generated coordinates stay in [0, extent): the cell-index range every hash/grid is valid for.
	Points makePoints(const unsigned seed, const size_t count, const float extent)
	{
		std::mt19937 rng(seed);
		std::uniform_real_distribution<float> dist(0.0f, extent);
		Points points;
		points.reserve(count);
		for (size_t i = 0; i < count; ++i) {
			const float x = dist(rng);
			const float y = dist(rng);
			const float z = dist(rng);
			points.emplace_back(x, y, z);
		}
		return points;
	}

	Points makeClusteredPoints(const unsigned seed, const size_t count, const float extent)
	{
		std::mt19937 rng(seed);
		std::uniform_real_distribution<float> center(0.25f * extent, 0.75f * extent);
		std::normal_distribution<float> jitter(0.0f, 0.02f * extent);
		const Vector3df c0(center(rng), center(rng), center(rng));
		const Vector3df c1(center(rng), center(rng), center(rng));
		Points points;
		for (size_t i = 0; i < count; ++i) {
			const auto& c = (i % 2 == 0) ? c0 : c1;
			points.emplace_back(c.x + jitter(rng), c.y + jitter(rng), c.z + jitter(rng));
		}
		return points;
	}

	// Same arithmetic as the production distance test, so boundary rounding cannot make the reference differ.
	float dist2(const Vector3df& a, const Vector3df& b) { return getDistanceSquared(a, b); }

	IdSet bruteRadius(const Points& points, const Vector3df& q, const float r, const bool inclusive, const int skip = -1)
	{
		IdSet out;
		for (size_t i = 0; i < points.size(); ++i) {
			if (static_cast<int>(i) == skip) continue;
			const float d2 = dist2(points[i], q);
			if (inclusive ? (d2 <= r * r) : (d2 < r * r)) out.push_back(static_cast<int>(i));
		}
		return out;
	}

	template <typename Container>
	IdSet normalized(const Container& c)
	{
		IdSet v(c.begin(), c.end());
		std::sort(v.begin(), v.end());
		return v;
	}

	bool hasDuplicates(IdSet v)
	{
		std::sort(v.begin(), v.end());
		return std::adjacent_find(v.begin(), v.end()) != v.end();
	}

	struct Scene { const char* name; Points points; };

	std::vector<Scene> makeScenes()
	{
		std::vector<Scene> scenes;
		scenes.push_back({ "uniform", makePoints(1u, 600, 10.0f) });
		scenes.push_back({ "cluster", makeClusteredPoints(2u, 600, 10.0f) });
		// 25% exact duplicates of earlier points.
		Points dup = makePoints(3u, 300, 10.0f);
		for (size_t i = 0; i < 100; ++i) dup.push_back(dup[i]);
		scenes.push_back({ "duplicate", dup });
		// Points on exact multiples of the cell size used below (1.0): cell-boundary coverage.
		Points grid;
		for (int x = 0; x < 6; ++x) for (int y = 0; y < 6; ++y) for (int z = 0; z < 6; ++z)
			grid.emplace_back(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
		scenes.push_back({ "lattice", grid });
		return scenes;
	}

	constexpr float kRadius = 1.0f;

	class TestItem : public ITreeItem
	{
	public:
		explicit TestItem(const Box3df& b) : box(b) {}
		Box3df getBox() override { return box; }
	private:
		Box3df box;
	};

	Box3df randomBox(std::mt19937& rng, const float extent, const float maxSize)
	{
		std::uniform_real_distribution<float> pos(0.0f, extent);
		std::uniform_real_distribution<float> size(0.0f, maxSize);
		const Vector3df lo(pos(rng), pos(rng), pos(rng));
		return Box3df(lo, Vector3df(lo.x + size(rng), lo.y + size(rng), lo.z + size(rng)));
	}
}

// ---------------------------------------------------------------- KDTree

TEST(DifferentialTest, KDTreeRadiusMatchesBruteForce)
{
	for (const auto& scene : makeScenes()) {
		SCOPED_TRACE(scene.name);
		KDTree tree;
		tree.build(scene.points);
		const auto queries = makePoints(11u, 60, 10.0f);
		for (const float r : { 0.0f, 0.3f, 1.0f, 2.5f }) {
			for (const auto& q : queries) {
				const auto got = tree.findWithinRadius(q, r);
				EXPECT_FALSE(hasDuplicates(got));
				EXPECT_EQ(normalized(got), normalized(bruteRadius(scene.points, q, r, true)));
			}
			// Query at each stored point itself: always found, even for r == 0.
			for (size_t i = 0; i < scene.points.size(); i += 17) {
				const auto got = normalized(tree.findWithinRadius(scene.points[i], r));
				EXPECT_TRUE(std::binary_search(got.begin(), got.end(), static_cast<int>(i)));
			}
		}
	}
}

TEST(DifferentialTest, KDTreeNearestAndKNearestMatchBruteForce)
{
	for (const auto& scene : makeScenes()) {
		SCOPED_TRACE(scene.name);
		KDTree tree;
		tree.build(scene.points);
		for (const auto& q : makePoints(12u, 60, 10.0f)) {
			std::vector<float> d;
			for (const auto& p : scene.points) d.push_back(dist2(p, q));
			std::sort(d.begin(), d.end());

			// Tie-breaking between equidistant points is unspecified, so compare distances, not ids.
			const int nearest = tree.findNearestIndex(q);
			ASSERT_GE(nearest, 0);
			EXPECT_FLOAT_EQ(dist2(scene.points[nearest], q), d[0]);
			const auto* np = tree.findNearest(q);
			ASSERT_NE(np, nullptr);
			EXPECT_FLOAT_EQ(dist2(*np, q), d[0]);

			for (const size_t k : { size_t(1), size_t(5), size_t(32), scene.points.size(), scene.points.size() + 10 }) {
				const auto ids = tree.findKNearestIndices(q, k);
				ASSERT_EQ(ids.size(), std::min(k, scene.points.size()));
				EXPECT_FALSE(hasDuplicates(ids));
				for (size_t i = 0; i < ids.size(); ++i) {
					EXPECT_FLOAT_EQ(dist2(scene.points[ids[i]], q), d[i]) << "rank " << i;
				}
			}
		}
	}
}

TEST(DifferentialTest, KDTreeEmptySingleAndRebuild)
{
	KDTree tree;
	tree.build();
	EXPECT_EQ(tree.findNearestIndex(Vector3df(0, 0, 0)), -1);
	EXPECT_EQ(tree.findNearest(Vector3df(0, 0, 0)), nullptr);
	EXPECT_TRUE(tree.findWithinRadius(Vector3df(0, 0, 0), 5.0f).empty());
	EXPECT_TRUE(tree.findKNearestIndices(Vector3df(0, 0, 0), 3).empty());

	tree.build(Points{ Vector3df(1, 2, 3) });
	EXPECT_EQ(tree.findNearestIndex(Vector3df(9, 9, 9)), 0);
	EXPECT_EQ(tree.findWithinRadius(Vector3df(1, 2, 3), 0.0f), IdSet{ 0 });
	EXPECT_TRUE(tree.findWithinRadius(Vector3df(1, 2, 3), -1.0f).empty());
	EXPECT_EQ(tree.findKNearestIndices(Vector3df(0, 0, 0), 0).size(), 0u);

	// Repeated rebuild with different data must not leak state from the previous tree.
	for (unsigned seed = 20; seed < 25; ++seed) {
		const auto pts = makePoints(seed, 100 + seed, 10.0f);
		tree.build(pts);
		const Vector3df q(5, 5, 5);
		EXPECT_EQ(normalized(tree.findWithinRadius(q, 2.0f)), normalized(bruteRadius(pts, q, 2.0f, true)));
	}
	tree.clear();
	EXPECT_EQ(tree.findNearestIndex(Vector3df(0, 0, 0)), -1);
}

TEST(DifferentialTest, KDTreeRadiusBoundaryIsInclusive)
{
	KDTree tree;
	tree.build(Points{ Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 2, 0) });
	EXPECT_EQ(normalized(tree.findWithinRadius(Vector3df(0, 0, 0), 1.0f)), (IdSet{ 0, 1 }));
	EXPECT_EQ(normalized(tree.findWithinRadius(Vector3df(0, 0, 0), 2.0f)), (IdSet{ 0, 1, 2 }));
}

// ---------------------------------------------------------------- CSR / sort-based searcher

TEST(DifferentialTest, CSRNeighborListMatchesBruteForceAndIsSymmetric)
{
	for (const auto& scene : makeScenes()) {
		SCOPED_TRACE(scene.name);
		CSRNeighborList list;
		list.build(scene.points, kRadius);
		ASSERT_EQ(list.size(), scene.points.size());
		for (size_t i = 0; i < scene.points.size(); ++i) {
			const auto row = list[i];
			const IdSet got(row.begin(), row.end());
			EXPECT_FALSE(hasDuplicates(got)) << "row " << i;
			EXPECT_EQ(normalized(got), normalized(bruteRadius(scene.points, scene.points[i], kRadius, false, static_cast<int>(i))))
				<< "row " << i;
			for (const int j : got) {
				const auto other = list[j];
				EXPECT_NE(std::find(other.begin(), other.end(), static_cast<int>(i)), other.end())
					<< j << " lacks reverse edge to " << i;
			}
		}
	}
}

TEST(DifferentialTest, CSRNeighborListRepeatedBuildReplacesPreviousState)
{
	CSRNeighborList list;
	for (unsigned seed = 30; seed < 34; ++seed) {
		const auto pts = makePoints(seed, 80 + 10 * (seed - 30), 6.0f);
		list.build(pts, kRadius);
		ASSERT_EQ(list.size(), pts.size());
		for (size_t i = 0; i < pts.size(); ++i) {
			const auto row = list[i];
			EXPECT_EQ(normalized(IdSet(row.begin(), row.end())),
				normalized(bruteRadius(pts, pts[i], kRadius, false, static_cast<int>(i))));
		}
	}
	list.clear();
	EXPECT_EQ(list.size(), 0u);
}

TEST(DifferentialTest, CSRNeighborListDegenerateInputs)
{
	CSRNeighborList list;
	list.build(Points{}, kRadius);
	EXPECT_EQ(list.size(), 0u);

	list.build(Points{ Vector3df(1, 1, 1) }, kRadius);
	ASSERT_EQ(list.size(), 1u);
	EXPECT_TRUE(list[0].empty());

	// Non-positive radius: rows exist but are empty.
	const auto pts = makePoints(40u, 20, 3.0f);
	for (const float r : { 0.0f, -1.0f }) {
		list.build(pts, r);
		ASSERT_EQ(list.size(), pts.size());
		for (size_t i = 0; i < pts.size(); ++i) EXPECT_TRUE(list[i].empty());
	}

	// Coincident points are neighbors of each other (distance 0 < r) but not of themselves.
	list.build(Points(3, Vector3df(2, 2, 2)), kRadius);
	for (size_t i = 0; i < 3; ++i) {
		const auto row = list[i];
		const IdSet got(row.begin(), row.end());
		EXPECT_EQ(got.size(), 2u);
		EXPECT_EQ(std::count(got.begin(), got.end(), static_cast<int>(i)), 0);
	}

	// A pair at exactly the radius is excluded (strict), just inside is included.
	list.build(Points{ Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 0.999f, 0) }, kRadius);
	EXPECT_EQ(normalized(IdSet(list[0].begin(), list[0].end())), IdSet{ 2 });
}

TEST(DifferentialTest, CSRNeighborListFromPairsStoresBothDirections)
{
	const std::vector<ParticlePair> pairs{ {0, 1}, {1, 2}, {0, 3} };
	CSRNeighborList list;
	list.build(5, pairs);
	ASSERT_EQ(list.size(), 5u);
	EXPECT_EQ(normalized(IdSet(list[0].begin(), list[0].end())), (IdSet{ 1, 3 }));
	EXPECT_EQ(normalized(IdSet(list[1].begin(), list[1].end())), (IdSet{ 0, 2 }));
	EXPECT_EQ(normalized(IdSet(list[2].begin(), list[2].end())), (IdSet{ 1 }));
	EXPECT_EQ(normalized(IdSet(list[3].begin(), list[3].end())), (IdSet{ 0 }));
	EXPECT_TRUE(list[4].empty());
}

TEST(DifferentialTest, SortBasedSearcherPairsMatchBruteForce)
{
	for (const auto& scene : makeScenes()) {
		SCOPED_TRACE(scene.name);
		IndexedSortBasedSearcher searcher(kRadius);
		searcher.add(scene.points);
		searcher.search();
		std::set<std::pair<int, int>> got;
		for (const auto& p : searcher.getPairs()) {
			EXPECT_NE(p.first, p.second);
			EXPECT_TRUE(got.insert({ std::min(p.first, p.second), std::max(p.first, p.second) }).second)
				<< "duplicate pair " << p.first << "," << p.second;
		}
		std::set<std::pair<int, int>> expected;
		for (size_t i = 0; i < scene.points.size(); ++i)
			for (size_t j = i + 1; j < scene.points.size(); ++j)
				if (dist2(scene.points[i], scene.points[j]) < kRadius * kRadius)
					expected.insert({ static_cast<int>(i), static_cast<int>(j) });
		EXPECT_EQ(got, expected);

		// search() replaces, never appends.
		searcher.search();
		EXPECT_EQ(searcher.getPairs().size(), expected.size());
	}
}

// ---------------------------------------------------------------- hashes / Z-indexed

TEST(DifferentialTest, SpaceHashMatchesBruteForce)
{
	for (const auto& scene : makeScenes()) {
		SCOPED_TRACE(scene.name);
		SpaceHash hash(kRadius, 1009);
		for (const auto& p : scene.points) hash.add(p);
		for (size_t i = 0; i < scene.points.size(); i += 3) {
			const auto got = hash.findNeighborIndices(scene.points[i]);
			// Strict radius, and the stored point at the query position is included.
			EXPECT_EQ(normalized(got), normalized(bruteRadius(scene.points, scene.points[i], kRadius, false)));
			EXPECT_FALSE(hasDuplicates(IdSet(got.begin(), got.end())));
		}
		for (const auto& q : makePoints(13u, 30, 10.0f)) {
			const auto got = hash.findNeighborIndices(q);
			EXPECT_EQ(normalized(got), normalized(bruteRadius(scene.points, q, kRadius, false)));
		}
	}
}

TEST(DifferentialTest, CompactSpaceHashMatchesBruteForce)
{
	for (const auto& scene : makeScenes()) {
		SCOPED_TRACE(scene.name);
		CompactSpaceHash hash(kRadius, 1009);
		for (const auto& p : scene.points) hash.add(p);
		for (size_t i = 0; i < scene.points.size(); i += 3) {
			const int idx = static_cast<int>(i);
			// By index: self excluded.
			const auto byIndex = hash.findNeighborIndices(idx);
			EXPECT_FALSE(hasDuplicates(byIndex));
			EXPECT_EQ(normalized(byIndex), normalized(bruteRadius(scene.points, scene.points[i], kRadius, false, idx)));
			// By position: self included.
			const auto byPos = hash.findNeighborIndices(scene.points[i]);
			EXPECT_FALSE(hasDuplicates(byPos));
			EXPECT_EQ(normalized(byPos), normalized(bruteRadius(scene.points, scene.points[i], kRadius, false)));
		}
	}
}

TEST(DifferentialTest, CompactSpaceHashRemoveExcludesRemovedPoints)
{
	const auto pts = makePoints(50u, 300, 8.0f);
	CompactSpaceHash hash(kRadius, 509);
	for (const auto& p : pts) hash.add(p);
	std::set<int> removed;
	for (int i = 0; i < 300; i += 4) { hash.remove(i); removed.insert(i); }

	for (int i = 1; i < 300; i += 7) {
		if (removed.count(i)) continue;
		IdSet expected;
		for (const int j : bruteRadius(pts, pts[i], kRadius, false, i))
			if (!removed.count(j)) expected.push_back(j);
		EXPECT_EQ(normalized(hash.findNeighborIndices(i)), normalized(expected));
	}
}

TEST(DifferentialTest, CompactSpaceHashFindReturnsExactCellContents)
{
	const auto pts = makePoints(51u, 200, 8.0f);
	CompactSpaceHash hash(kRadius, 509);
	for (const auto& p : pts) hash.add(p);
	for (int i = 0; i < 200; i += 5) {
		const auto cell = hash.toIndex(pts[i]);
		IdSet expected;
		for (int j = 0; j < 200; ++j) if (hash.toIndex(pts[j]) == cell) expected.push_back(j);
		EXPECT_EQ(normalized(hash.find(i)), normalized(expected));
		EXPECT_EQ(normalized(hash.find(cell)), normalized(expected));
		EXPECT_FALSE(hash.isEmpty(pts[i]));
	}
	EXPECT_TRUE(hash.isEmpty(Vector3df(500.0f, 500.0f, 500.0f)));
}

TEST(DifferentialTest, ZIndexedSearcherMatchesBruteForce)
{
	for (const auto& scene : makeScenes()) {
		SCOPED_TRACE(scene.name);
		ZIndexedSearcher searcher(kRadius, Vector3df(0, 0, 0));
		for (const auto& p : scene.points) searcher.add(p);
		searcher.sort();
		for (size_t i = 0; i < scene.points.size(); i += 3) {
			const auto got = searcher.findNeighbors(scene.points[i]);
			EXPECT_FALSE(hasDuplicates(IdSet(got.begin(), got.end())));
			EXPECT_EQ(normalized(got), normalized(bruteRadius(scene.points, scene.points[i], kRadius, false)));
		}
	}
}

// ---------------------------------------------------------------- Octree

TEST(DifferentialTest, OctreeBoxQueryMatchesBruteForce)
{
	std::mt19937 rng(60u);
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	std::vector<std::unique_ptr<TestItem>> items;
	Octree tree(domain);
	for (int i = 0; i < 300; ++i) {
		items.push_back(std::make_unique<TestItem>(randomBox(rng, 9.0f, 1.5f)));
		tree.add(items.back().get());
	}
	for (int q = 0; q < 80; ++q) {
		const Box3df query = randomBox(rng, 10.0f, 4.0f);
		std::set<ITreeItem*> got;
		for (auto* item : tree.findItems(query)) EXPECT_TRUE(got.insert(item).second) << "duplicate item";
		std::set<ITreeItem*> expected;
		for (auto& item : items) if (domain.intersects(item->getBox()) && item->getBox().intersects(query)) expected.insert(item.get());
		EXPECT_EQ(got, expected);
	}
}

TEST(DifferentialTest, OctreeSphereQueryMatchesBruteForce)
{
	std::mt19937 rng(61u);
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	std::vector<std::unique_ptr<TestItem>> items;
	Octree tree(domain);
	for (int i = 0; i < 300; ++i) {
		items.push_back(std::make_unique<TestItem>(randomBox(rng, 9.0f, 1.0f)));
		tree.add(items.back().get());
	}
	std::uniform_real_distribution<float> pos(0.0f, 10.0f);
	for (int q = 0; q < 80; ++q) {
		const Vector3df c(pos(rng), pos(rng), pos(rng));
		const float r = 0.2f + 0.05f * q;
		std::set<ITreeItem*> got;
		for (auto* item : tree.findItems(c, r)) EXPECT_TRUE(got.insert(item).second) << "duplicate item";
		// Item AABB vs sphere (distance from center to the box), the documented semantics.
		std::set<ITreeItem*> expected;
		for (auto& item : items) {
			const auto b = item->getBox();
			const auto lo = b.getMin();
			const auto hi = b.getMax();
			const float dx = std::max({ lo.x - c.x, 0.0f, c.x - hi.x });
			const float dy = std::max({ lo.y - c.y, 0.0f, c.y - hi.y });
			const float dz = std::max({ lo.z - c.z, 0.0f, c.z - hi.z });
			if (!domain.intersects(b)) continue;
			if (dx * dx + dy * dy + dz * dz <= r * r) expected.insert(item.get());
		}
		// Float rounding at the exact boundary may differ by the tolerance; tolerate only boundary items.
		for (auto* item : expected) EXPECT_TRUE(got.count(item)) << "missed item at q=" << q;
		for (auto* item : got) {
			if (expected.count(item)) continue;
			const auto b = item->getBox();
			const auto lo = b.getMin();
			const auto hi = b.getMax();
			const float dx = std::max({ lo.x - c.x, 0.0f, c.x - hi.x });
			const float dy = std::max({ lo.y - c.y, 0.0f, c.y - hi.y });
			const float dz = std::max({ lo.z - c.z, 0.0f, c.z - hi.z });
			EXPECT_NEAR(std::sqrt(dx * dx + dy * dy + dz * dz), r, 1.0e-4f) << "extra non-boundary item at q=" << q;
		}
	}
}

TEST(DifferentialTest, OctreeEmptyAndOutOfDomain)
{
	const Box3df domain(Vector3df(0, 0, 0), Vector3df(10, 10, 10));
	Octree tree(domain);
	EXPECT_TRUE(tree.findItems(domain).empty());
	EXPECT_TRUE(tree.findItems(Vector3df(5, 5, 5), 100.0f).empty());

	// An item entirely outside the root box is silently ignored (documented contract).
	TestItem outside(Box3df(Vector3df(20, 20, 20), Vector3df(21, 21, 21)));
	tree.add(&outside);
	EXPECT_TRUE(tree.findItems(Box3df(Vector3df(-100, -100, -100), Vector3df(100, 100, 100))).empty());

	// Degenerate (zero-size) item box is found by a covering query.
	TestItem point(Box3df(Vector3df(3, 3, 3), Vector3df(3, 3, 3)));
	tree.add(&point);
	const auto hits = tree.findItems(Box3df(Vector3df(2, 2, 2), Vector3df(4, 4, 4)));
	ASSERT_EQ(hits.size(), 1u);
	EXPECT_EQ(hits.front(), &point);
}

// ---------------------------------------------------------------- BVH

namespace {
	struct BvhScene
	{
		std::vector<std::unique_ptr<BVHObject>> owned;
		std::vector<BVHObject*> ptrs;
	};

	BvhScene makeBvhScene(const unsigned seed, const int count)
	{
		std::mt19937 rng(seed);
		BvhScene s;
		for (int i = 0; i < count; ++i) {
			s.owned.push_back(std::make_unique<BVHObject>(i, randomBox(rng, 10.0f, 1.2f)));
			s.ptrs.push_back(s.owned.back().get());
		}
		return s;
	}

	std::set<std::pair<int, int>> brutePairs(const BvhScene& s)
	{
		std::set<std::pair<int, int>> out;
		for (size_t i = 0; i < s.ptrs.size(); ++i)
			for (size_t j = i + 1; j < s.ptrs.size(); ++j)
				if (s.ptrs[i]->box.intersects(s.ptrs[j]->box))
					out.insert({ std::min(s.ptrs[i]->id, s.ptrs[j]->id), std::max(s.ptrs[i]->id, s.ptrs[j]->id) });
		return out;
	}

	void expectBvhMatches(const BvhScene& s, const BVH& bvh, std::mt19937& rng)
	{
		for (int q = 0; q < 40; ++q) {
			const Box3df query = randomBox(rng, 10.0f, 3.0f);
			IdSet got;
			for (auto* o : bvh.queryOverlaps(query)) got.push_back(o->id);
			EXPECT_FALSE(hasDuplicates(got));
			IdSet expected;
			for (auto* o : s.ptrs) if (o->box.intersects(query)) expected.push_back(o->id);
			EXPECT_EQ(normalized(got), normalized(expected));
		}

		const auto pairs = bvh.findAllPairs();
		std::set<std::pair<int, int>> got;
		for (const auto& p : pairs) {
			EXPECT_LT(p.first, p.second);
			EXPECT_TRUE(got.insert(p).second) << "duplicate pair " << p.first << "," << p.second;
		}
		EXPECT_EQ(got, brutePairs(s));
	}
}

TEST(DifferentialTest, BVHOverlapsAndPairsMatchBruteForce)
{
	for (const int leafMax : { 1, 4, 16 }) {
		SCOPED_TRACE(leafMax);
		auto scene = makeBvhScene(70u, 250);
		BVH bvh(scene.ptrs, leafMax);
		std::mt19937 rng(71u);
		expectBvhMatches(scene, bvh, rng);
	}
}

TEST(DifferentialTest, BVHRefitAfterMovingObjectsMatchesBruteForce)
{
	auto scene = makeBvhScene(72u, 250);
	BVH bvh(scene.ptrs, 4);
	std::mt19937 rng(73u);
	for (int round = 0; round < 4; ++round) {
		SCOPED_TRACE(round);
		std::uniform_real_distribution<float> move(-2.0f, 2.0f);
		for (auto* o : scene.ptrs) {
			const Vector3df d(move(rng), move(rng), move(rng));
			o->box = Box3df(o->box.getMin() + d, o->box.getMax() + d);
		}
		bvh.refit();
		expectBvhMatches(scene, bvh, rng);
	}
}

TEST(DifferentialTest, BVHRayCandidatesCoverEveryTrueHit)
{
	auto scene = makeBvhScene(74u, 250);
	BVH bvh(scene.ptrs, 4);
	std::mt19937 rng(75u);
	std::uniform_real_distribution<float> pos(-2.0f, 12.0f);
	std::uniform_real_distribution<float> comp(0.1f, 1.0f);
	std::bernoulli_distribution flip(0.5);
	for (int q = 0; q < 80; ++q) {
		const Vector3df o(pos(rng), pos(rng), pos(rng));
		// Non-axis-parallel, deliberately non-normalized direction (t scales with |d|).
		const Vector3df d(comp(rng) * (flip(rng) ? 3.0f : -3.0f), comp(rng) * (flip(rng) ? 3.0f : -3.0f), comp(rng) * (flip(rng) ? 3.0f : -3.0f));
		const float tMin = 1.0e-4f;
		const float tMax = 1.0e38f;

		auto slab = [&](const Box3df& b) {
			float lo = tMin, hi = tMax;
			for (int i = 0; i < 3; ++i) {
				float t0 = (b.getMin()[i] - o[i]) / d[i];
				float t1 = (b.getMax()[i] - o[i]) / d[i];
				if (t0 > t1) std::swap(t0, t1);
				lo = std::max(lo, t0);
				hi = std::min(hi, t1);
			}
			return lo <= hi;
		};
		IdSet expected;
		for (auto* obj : scene.ptrs) if (slab(obj->box)) expected.push_back(obj->id);
		IdSet got;
		for (auto* obj : bvh.queryRay(o, d, tMin, tMax)) got.push_back(obj->id);
		EXPECT_FALSE(hasDuplicates(got));
		EXPECT_EQ(normalized(got), normalized(expected)) << "ray " << q;
	}
}

TEST(DifferentialTest, BVHEmptyAndSingleObject)
{
	BVH empty(std::vector<BVHObject*>{}, 4);
	EXPECT_EQ(empty.nodeCount(), 0);
	EXPECT_TRUE(empty.queryOverlaps(Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1))).empty());
	EXPECT_TRUE(empty.queryRay(Vector3df(0, 0, 0), Vector3df(1, 1, 1)).empty());
	EXPECT_TRUE(empty.findAllPairs().empty());
	empty.refit(); // must be a no-op

	BVHObject single(7, Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1)));
	BVH bvh(std::vector<BVHObject*>{ &single }, 4);
	EXPECT_EQ(bvh.queryOverlaps(Box3df(Vector3df(0.5f, 0.5f, 0.5f), Vector3df(2, 2, 2))).size(), 1u);
	EXPECT_TRUE(bvh.queryOverlaps(Box3df(Vector3df(2, 2, 2), Vector3df(3, 3, 3))).empty());
	EXPECT_TRUE(bvh.findAllPairs().empty());
}

TEST(DifferentialTest, BVHTouchingBoxesCountAsOverlapping)
{
	BVHObject a(0, Box3df(Vector3df(0, 0, 0), Vector3df(1, 1, 1)));
	BVHObject b(1, Box3df(Vector3df(1, 0, 0), Vector3df(2, 1, 1)));
	BVHObject c(2, Box3df(Vector3df(1.01f, 0, 0), Vector3df(2, 1, 1)));
	BVH bvh(std::vector<BVHObject*>{ &a, &b, &c }, 1);
	const auto pairs = bvh.findAllPairs();
	const std::set<std::pair<int, int>> got(pairs.begin(), pairs.end());
	EXPECT_EQ(got, (std::set<std::pair<int, int>>{ {0, 1}, {1, 2} }));
}

// ---------------------------------------------------------------- coordinate range
// The packed gridId is linear in the cell index, so negative and wide-X coordinates only add false
// *candidates* (removed by the distance filter) and do not lose neighbors. These pin that down.

TEST(DifferentialTest, CSRNeighborListHandlesNegativeCoordinates)
{
	auto pts = makePoints(80u, 400, 6.0f);
	for (auto& p : pts) { p.x -= 3.0f; p.y -= 3.0f; p.z -= 3.0f; }
	CSRNeighborList list;
	list.build(pts, kRadius);
	for (size_t i = 0; i < pts.size(); ++i) {
		const auto row = list[i];
		EXPECT_EQ(normalized(IdSet(row.begin(), row.end())),
			normalized(bruteRadius(pts, pts[i], kRadius, false, static_cast<int>(i)))) << "row " << i;
	}
}

TEST(DifferentialTest, CSRNeighborListHandlesWideXRangePastGridStride)
{
	// 2000 cells wide on X exceeds the 10-bit packed-gridId stride.
	std::mt19937 rng(81u);
	std::uniform_real_distribution<float> x(0.0f, 2000.0f), yz(0.0f, 3.0f);
	Points pts;
	for (int i = 0; i < 4000; ++i) pts.emplace_back(x(rng), yz(rng), yz(rng));
	CSRNeighborList list;
	list.build(pts, kRadius);
	for (size_t i = 0; i < pts.size(); i += 11) {
		const auto row = list[i];
		EXPECT_EQ(normalized(IdSet(row.begin(), row.end())),
			normalized(bruteRadius(pts, pts[i], kRadius, false, static_cast<int>(i)))) << "row " << i;
	}
}

TEST(DifferentialTest, CompactSpaceHashHandlesNegativeCoordinates)
{
	auto pts = makePoints(82u, 400, 6.0f);
	for (auto& p : pts) { p.x -= 3.0f; p.y -= 3.0f; p.z -= 3.0f; }
	CompactSpaceHash hash(kRadius, 509);
	for (const auto& p : pts) hash.add(p);
	for (int i = 0; i < 400; i += 5) {
		EXPECT_EQ(normalized(hash.findNeighborIndices(i)), normalized(bruteRadius(pts, pts[i], kRadius, false, i)));
	}
}

// gridId = iz * 2^20 + ... used to overflow int (signed UB) for iz >= 2048; it is 64-bit now.
TEST(DifferentialTest, CSRNeighborListHandlesZCellIndexBeyondIntRange)
{
	std::mt19937 rng(83u);
	std::uniform_real_distribution<float> z(0.0f, 3000.0f), xy(0.0f, 3.0f);
	Points pts;
	for (int i = 0; i < 4000; ++i) pts.emplace_back(xy(rng), xy(rng), z(rng));
	CSRNeighborList list;
	list.build(pts, kRadius);
	for (size_t i = 0; i < pts.size(); i += 11) {
		const auto row = list[i];
		EXPECT_EQ(normalized(IdSet(row.begin(), row.end())),
			normalized(bruteRadius(pts, pts[i], kRadius, false, static_cast<int>(i)))) << "row " << i;
	}
}
