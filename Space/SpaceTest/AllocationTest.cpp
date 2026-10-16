// Allocation counting for the repeated build / query patterns (Phase 3). The global operator new/delete
// are replaced for the whole SpaceTest executable with counting versions; the counter is only read as a
// delta around the code under test.
#include "gtest/gtest.h"

#include "../Core/BVH.h"
#include "../Core/CompactSpaceHash.h"
#include "../Core/KDTree.h"
#include "../Core/CSRNeighborList.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <random>
#include <vector>

namespace {
	std::atomic<unsigned long long> g_allocationCount{ 0 };
}

void* operator new(std::size_t size)
{
	g_allocationCount.fetch_add(1, std::memory_order_relaxed);
	if (void* p = std::malloc(size ? size : 1)) return p;
	std::abort(); // exceptions are not used in this code base
}

void* operator new[](std::size_t size)
{
	g_allocationCount.fetch_add(1, std::memory_order_relaxed);
	if (void* p = std::malloc(size ? size : 1)) return p;
	std::abort();
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {

	unsigned long long allocations() { return g_allocationCount.load(std::memory_order_relaxed); }

	std::vector<Vector3df> makePoints(const unsigned seed, const size_t count, const float extent)
	{
		std::mt19937 rng(seed);
		std::uniform_real_distribution<float> d(0.0f, extent);
		std::vector<Vector3df> pts;
		for (size_t i = 0; i < count; ++i) { const float x = d(rng), y = d(rng), z = d(rng); pts.emplace_back(x, y, z); }
		return pts;
	}

	// Allocations made by one call of `f`.
	template <typename F>
	unsigned long long countAllocations(F&& f)
	{
		const auto before = allocations();
		f();
		return allocations() - before;
	}

	// The OpenMP runtime may allocate a handful of bytes per parallel region (the Debug runtime does:
	// 7 per CSR build, Release 0). That is a constant of the runtime, not of the data size -- before
	// workspace reuse a rebuild made ~190 allocations of our own -- so a rebuild is held to this bound.
	constexpr unsigned long long kRuntimeAllocationAllowance = 16;
}

// Steady state = the same call repeated after one warm-up call, so buffers have reached their size.

TEST(AllocationTest, CSRNeighborListRebuildAllocatesNothingAfterWarmUp)
{
	const auto pts = makePoints(1u, 20000, 20.0f);
	CSRNeighborList list;
	const auto first = countAllocations([&] { list.build(pts, 1.0f); });
	EXPECT_GT(first, 0u);
	// Before workspace reuse this was ~190 allocations per rebuild (per-thread pair lists regrowing).
	EXPECT_LE(countAllocations([&] { list.build(pts, 1.0f); }), kRuntimeAllocationAllowance);
	EXPECT_LE(countAllocations([&] { list.build(pts, 1.0f); }), kRuntimeAllocationAllowance);

	// A smaller problem fits in the existing capacity too.
	const auto fewer = makePoints(5u, 8000, 20.0f);
	EXPECT_LE(countAllocations([&] { list.build(fewer, 1.0f); }), kRuntimeAllocationAllowance);

	// tryBuild shares the scratch.
	EXPECT_LE(countAllocations([&] { EXPECT_TRUE(list.tryBuild(fewer, 1.0f)); }), kRuntimeAllocationAllowance);
}

TEST(AllocationTest, ReleaseScratchGivesMemoryBack)
{
	const auto pts = makePoints(1u, 20000, 20.0f);
	CSRNeighborList list;
	list.build(pts, 1.0f);
	ASSERT_LE(countAllocations([&] { list.build(pts, 1.0f); }), kRuntimeAllocationAllowance);

	list.releaseScratch();
	EXPECT_GT(countAllocations([&] { list.build(pts, 1.0f); }), kRuntimeAllocationAllowance) << "scratch was freed, so the next build allocates again";
	EXPECT_LE(countAllocations([&] { list.build(pts, 1.0f); }), kRuntimeAllocationAllowance);
}

TEST(AllocationTest, KDTreeBufferQueriesAllocateNothingAfterWarmUp)
{
	const auto pts = makePoints(2u, 5000, 10.0f);
	KDTree tree;
	tree.build(pts);
	const Vector3df q(5, 5, 5);

	std::vector<int> out;
	KDTree::KNearestScratch scratch;
	tree.findWithinRadius(q, 1.0f, out);
	tree.findKNearestIndices(q, 16, out, scratch);
	EXPECT_EQ(countAllocations([&] { tree.findWithinRadius(q, 1.0f, out); }), 0u);
	EXPECT_EQ(countAllocations([&] { tree.findKNearestIndices(q, 16, out, scratch); }), 0u);
	EXPECT_EQ(countAllocations([&] { tree.findKNearestIndices(q, 4, out, scratch); }), 0u) << "smaller k reuses the heap";

	size_t visited = 0;
	EXPECT_EQ(countAllocations([&] { tree.forEachWithinRadius(q, 1.0f, [&](int) { ++visited; }); }), 0u);
	EXPECT_GT(visited, 0u);
}

TEST(AllocationTest, CompactSpaceHashBufferQueriesAllocateNothingAfterWarmUp)
{
	const auto pts = makePoints(2u, 5000, 10.0f);
	CompactSpaceHash hash(1.0, 10007);
	for (const auto& p : pts) hash.add(p);

	std::vector<int> out;
	hash.findNeighborIndices(100, out);
	EXPECT_EQ(countAllocations([&] { for (int i = 0; i < 200; ++i) hash.findNeighborIndices(i, out); }), 0u);
	EXPECT_EQ(countAllocations([&] { hash.findNeighborIndices(Vector3df(5, 5, 5), out); }), 0u);

	size_t visited = 0;
	EXPECT_EQ(countAllocations([&] { for (int i = 0; i < 200; ++i) hash.forEachNeighbor(i, [&](int) { ++visited; }); }), 0u);
	EXPECT_GT(visited, 0u);
}

TEST(AllocationTest, BVHBufferQueriesAllocateNothingAfterWarmUp)
{
	const auto pts = makePoints(2u, 5000, 10.0f);
	std::vector<std::unique_ptr<BVHObject>> owned;
	std::vector<BVHObject*> ptrs;
	for (size_t i = 0; i < pts.size(); ++i) {
		owned.push_back(std::make_unique<BVHObject>(static_cast<int>(i), Box3df(pts[i], pts[i] + Vector3df(0.2f, 0.2f, 0.2f))));
		ptrs.push_back(owned.back().get());
	}
	BVH bvh(ptrs, 4);

	std::vector<BVHObject*> hits;
	std::vector<std::pair<int, int>> pairs, stack;
	const Box3df box(Vector3df(4, 4, 4), Vector3df(6, 6, 6));
	bvh.queryOverlaps(box, hits);
	bvh.queryRay(Vector3df(0, 5, 5), Vector3df(1, 0.01f, 0.01f), hits);
	bvh.findAllPairs(pairs, stack);

	EXPECT_EQ(countAllocations([&] { bvh.queryOverlaps(box, hits); }), 0u);
	EXPECT_EQ(countAllocations([&] { bvh.queryRay(Vector3df(0, 5, 5), Vector3df(1, 0.01f, 0.01f), hits); }), 0u);
	EXPECT_EQ(countAllocations([&] { bvh.findAllPairs(pairs, stack); }), 0u);
	size_t visited = 0;
	EXPECT_EQ(countAllocations([&] { bvh.forEachOverlap(box, [&](BVHObject*) { ++visited; }); }), 0u);
	EXPECT_EQ(countAllocations([&] { bvh.forEachRayCandidate(Vector3df(0, 5, 5), Vector3df(1, 0.01f, 0.01f), 1e-4f, 1e38f, [&](BVHObject*) { ++visited; }); }), 0u);
	EXPECT_GT(visited, 0u);
}
