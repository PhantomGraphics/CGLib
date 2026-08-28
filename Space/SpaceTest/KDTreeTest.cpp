#include "gtest/gtest.h"
#include "../Space/KDTree.h"

#include <algorithm>

using namespace Phantom::Math;
using namespace Phantom::Space;

TEST(KDTreeTest, EmptyTree)
{
	KDTree kd;
	EXPECT_EQ(kd.findNearest(Vector3df(0.0f, 0.0f, 0.0f)), nullptr);
	auto res = kd.findWithinRadius(Vector3df(0.0f, 0.0f, 0.0f), 1.0f);
	EXPECT_TRUE(res.empty());
}

TEST(KDTreeTest, SinglePoint)
{
	KDTree kd;
	const Vector3df p(1.0f, 2.0f, 3.0f);
	kd.addPoint(p);
	kd.build();

	auto out = kd.findNearest(Vector3df(1.0f, 2.0f, 3.0f));
	EXPECT_TRUE(areSame(*out, p, 1.0e-6f));

	auto indices = kd.findWithinRadius(Vector3df(1.0f, 2.0f, 3.0f), 0.0f);
	EXPECT_EQ(indices.size(), 1u);
	EXPECT_EQ(indices[0], 0);
}

TEST(KDTreeTest, NearestAndRadius)
{
	KDTree kd;
	std::vector<Vector3df> pts = {
		Vector3df(0.0f, 0.0f, 0.0f), // idx 0
		Vector3df(1.0f, 0.0f, 0.0f), // idx 1
		Vector3df(5.0f, 0.0f, 0.0f), // idx 2
		Vector3df(0.0f, 3.0f, 0.0f)  // idx 3
	};
	for (const auto& p : pts) {
		kd.addPoint(p);
	}
	kd.build();

	Vector3df query(0.9f, 0.0f, 0.0f);
	auto nearest = kd.findNearest(query);
	ASSERT_NE(nearest, nullptr);
	EXPECT_TRUE(areSame(*nearest, pts[1], 1.0e-6f));

	auto within = kd.findWithinRadius(query, 1.1f);
	std::sort(within.begin(), within.end());
	std::vector<int> expected = { 0, 1 };
	EXPECT_EQ(within, expected);
}

TEST(KDTreeTest, DuplicatePoints)
{
	KDTree kd;
	const Vector3df p(1.0f, 1.0f, 1.0f);
	kd.addPoint(p);
	kd.addPoint(p); // duplicate
	kd.build();

	auto nearest = kd.findNearest(p);
	ASSERT_NE(nearest, nullptr);
	EXPECT_TRUE(areSame(*nearest, p, 1.0e-6f));

	auto within = kd.findWithinRadius(p, 0.0f);
	// both exact duplicates should be returned when radius == 0
	EXPECT_EQ(within.size(), 2u);
}

TEST(KDTreeTest, FindWithinRadius_Empty)
{
	KDTree kd;
	auto within = kd.findWithinRadius(Vector3df(0.0f, 0.0f, 0.0f), 10.0f);
	EXPECT_TRUE(within.empty());
}

TEST(KDTreeTest, FindNearestIndex_ReturnsInsertionOrderIndex)
{
	KDTree kd;
	std::vector<Vector3df> pts = {
		Vector3df(0.0f, 0.0f, 0.0f), // idx 0
		Vector3df(1.0f, 0.0f, 0.0f), // idx 1
		Vector3df(5.0f, 0.0f, 0.0f)  // idx 2
	};
	for (const auto& p : pts) {
		kd.addPoint(p);
	}
	kd.build();

	EXPECT_EQ(kd.findNearestIndex(Vector3df(0.9f, 0.0f, 0.0f)), 1);
}

TEST(KDTreeTest, FindNearestIndex_EmptyTreeReturnsNegativeOne)
{
	KDTree kd;
	EXPECT_EQ(kd.findNearestIndex(Vector3df(0.0f, 0.0f, 0.0f)), -1);
}

TEST(KDTreeTest, Clear_ResetsTreeToEmptyState)
{
	KDTree kd;
	kd.addPoint(Vector3df(1.0f, 2.0f, 3.0f));
	kd.build();
	ASSERT_NE(kd.findNearest(Vector3df(1.0f, 2.0f, 3.0f)), nullptr);

	kd.clear();

	EXPECT_EQ(kd.findNearest(Vector3df(1.0f, 2.0f, 3.0f)), nullptr);
	EXPECT_EQ(kd.findNearestIndex(Vector3df(1.0f, 2.0f, 3.0f)), -1);
	EXPECT_TRUE(kd.findWithinRadius(Vector3df(1.0f, 2.0f, 3.0f), 10.0f).empty());
}

TEST(KDTreeTest, FindKNearestIndices_ReturnsSortedByAscendingDistance)
{
	KDTree kd;
	std::vector<Vector3df> pts = {
		Vector3df(0.0f, 0.0f, 0.0f), // idx 0, dist 5
		Vector3df(1.0f, 0.0f, 0.0f), // idx 1, dist 4
		Vector3df(5.0f, 0.0f, 0.0f), // idx 2, dist 0
		Vector3df(0.0f, 3.0f, 0.0f)  // idx 3, dist sqrt(25+9)
	};
	for (const auto& p : pts) {
		kd.addPoint(p);
	}
	kd.build();

	const auto result = kd.findKNearestIndices(Vector3df(5.0f, 0.0f, 0.0f), 3);
	ASSERT_EQ(result.size(), 3u);
	EXPECT_EQ(result[0], 2);
	EXPECT_EQ(result[1], 1);
	EXPECT_EQ(result[2], 0);
}

TEST(KDTreeTest, FindKNearestIndices_KLargerThanPointCountReturnsAll)
{
	KDTree kd;
	std::vector<Vector3df> pts = {
		Vector3df(0.0f, 0.0f, 0.0f),
		Vector3df(1.0f, 0.0f, 0.0f)
	};
	for (const auto& p : pts) {
		kd.addPoint(p);
	}
	kd.build();

	const auto result = kd.findKNearestIndices(Vector3df(0.0f, 0.0f, 0.0f), 10);
	EXPECT_EQ(result.size(), 2u);
}

TEST(KDTreeTest, FindKNearestIndices_KZeroOrEmptyTreeReturnsEmpty)
{
	KDTree kd;
	kd.addPoint(Vector3df(1.0f, 2.0f, 3.0f));
	kd.build();

	EXPECT_TRUE(kd.findKNearestIndices(Vector3df(1.0f, 2.0f, 3.0f), 0).empty());

	KDTree empty;
	EXPECT_TRUE(empty.findKNearestIndices(Vector3df(0.0f, 0.0f, 0.0f), 3).empty());
}

TEST(KDTreeTest, FindKNearestIndices_MatchesFindNearestForKOne)
{
	KDTree kd;
	std::vector<Vector3df> pts = {
		Vector3df(0.0f, 0.0f, 0.0f),
		Vector3df(1.0f, 0.0f, 0.0f),
		Vector3df(5.0f, 0.0f, 0.0f),
		Vector3df(0.0f, 3.0f, 0.0f)
	};
	for (const auto& p : pts) {
		kd.addPoint(p);
	}
	kd.build();

	const Vector3df query(0.9f, 0.0f, 0.0f);
	const auto result = kd.findKNearestIndices(query, 1);
	ASSERT_EQ(result.size(), 1u);
	EXPECT_EQ(result[0], kd.findNearestIndex(query));
}

TEST(KDTreeTest, FindNearest_Collinear)
{
	KDTree kd;
	std::vector<Vector3df> pts = {
		Vector3df(0.0f, 0.0f, 0.0f),
		Vector3df(2.0f, 0.0f, 0.0f),
		Vector3df(4.0f, 0.0f, 0.0f),
		Vector3df(6.0f, 0.0f, 0.0f)
	};

	for (const auto& p : pts) {
		kd.addPoint(p);
	}
	kd.build();

	auto nearest = kd.findNearest(Vector3df(3.1f, 0.0f, 0.0f));
	ASSERT_NE(nearest, nullptr);
	EXPECT_TRUE(areSame(*nearest, Vector3df(4.0f, 0.0f, 0.0f), 1.0e-6f));

	auto within = kd.findWithinRadius(Vector3df(3.1f, 0.0f, 0.0f), 1.2f);
	std::sort(within.begin(), within.end());
	std::vector<int> expected = { 1, 2 };
	EXPECT_EQ(within, expected);
}

TEST(KDTreeTest, BuildFromVector_MatchesAddPointBuild)
{
	std::vector<Vector3df> pts = {
		Vector3df(0.0f, 0.0f, 0.0f),
		Vector3df(1.0f, 0.0f, 0.0f),
		Vector3df(5.0f, 0.0f, 0.0f),
		Vector3df(0.0f, 3.0f, 0.0f)
	};

	KDTree kd;
	kd.build(pts);

	const Vector3df query(0.9f, 0.0f, 0.0f);
	EXPECT_EQ(kd.findNearestIndex(query), 1);

	auto within = kd.findWithinRadius(query, 1.1f);
	std::sort(within.begin(), within.end());
	std::vector<int> expected = { 0, 1 };
	EXPECT_EQ(within, expected);
}
