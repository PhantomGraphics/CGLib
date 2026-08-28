#include "gtest/gtest.h"

#include "../Space/CompactSpaceHash.h"

#include <algorithm>

using namespace Phantom::Math;
using namespace Phantom::Space;

TEST(CompactSpaceHashTest, TestGetNeighborIndices)
{
	CompactSpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0, 0, 0));
	spaceHash.add(Vector3df(0.5, 0, 0));

	const auto actual = spaceHash.findNeighborIndices(0);
	EXPECT_EQ(1, actual.size());
}

TEST(CompactSpaceHashTest, GetNeighbors_VerifyIndices)
{
	CompactSpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0.0f, 0.0f, 0.0f));   // index 0
	spaceHash.add(Vector3df(0.5f, 0.0f, 0.0f));   // index 1
	spaceHash.add(Vector3df(2.0f, 0.0f, 0.0f));   // index 2

	auto neighbors = spaceHash.findNeighborIndices(0);
	std::sort(neighbors.begin(), neighbors.end());

	ASSERT_EQ(1u, neighbors.size());
	EXPECT_EQ(1, neighbors[0]);
}

TEST(CompactSpaceHashTest, GetNeighbors_AcrossCellBoundary)
{
	CompactSpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0.99f, 0.0f, 0.0f));  // index 0, cell x=0
	spaceHash.add(Vector3df(1.01f, 0.0f, 0.0f));  // index 1, cell x=1

	auto neighbors0 = spaceHash.findNeighborIndices(0);
	std::sort(neighbors0.begin(), neighbors0.end());
	ASSERT_EQ(1u, neighbors0.size());
	EXPECT_EQ(1, neighbors0[0]);

	auto neighbors1 = spaceHash.findNeighborIndices(1);
	std::sort(neighbors1.begin(), neighbors1.end());
	ASSERT_EQ(1u, neighbors1.size());
	EXPECT_EQ(0, neighbors1[0]);
}

TEST(CompactSpaceHashTest, TestRemove)
{
	CompactSpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0, 0, 0));
	spaceHash.add(Vector3df(0.5, 0, 0));

	spaceHash.remove(0);
   const auto actual = spaceHash.findNeighborIndices(1);
	EXPECT_TRUE(actual.empty());

}

TEST(CompactSpaceHashTest, GetNeighbors_ByPosition_ExcludesFarPoints)
{
	CompactSpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0.0f, 0.0f, 0.0f));   // index 0
	spaceHash.add(Vector3df(0.5f, 0.0f, 0.0f));   // index 1
	spaceHash.add(Vector3df(2.0f, 0.0f, 0.0f));   // index 2 (too far)

	auto neighbors = spaceHash.findNeighborIndices(Vector3df(0.0f, 0.0f, 0.0f));
	std::sort(neighbors.begin(), neighbors.end());

	ASSERT_EQ(2u, neighbors.size());
	EXPECT_EQ(0, neighbors[0]);
	EXPECT_EQ(1, neighbors[1]);
}

TEST(CompactSpaceHashTest, Find_ByIndexAndByGridPositionAgree)
{
	CompactSpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0.2f, 0.0f, 0.0f));   // index 0
	spaceHash.add(Vector3df(0.7f, 0.0f, 0.0f));   // index 1, same cell as index 0

	auto byIndex = spaceHash.find(0);
	auto byGrid = spaceHash.find(spaceHash.toIndex(Vector3df(0.2f, 0.0f, 0.0f)));
	std::sort(byIndex.begin(), byIndex.end());
	std::sort(byGrid.begin(), byGrid.end());

	EXPECT_EQ(byIndex, byGrid);
	ASSERT_EQ(2u, byIndex.size());
	EXPECT_EQ(0, byIndex[0]);
	EXPECT_EQ(1, byIndex[1]);
}

TEST(CompactSpaceHashTest, IsEmpty_TrueForUnoccupiedCell_FalseAfterAdd)
{
	CompactSpaceHash spaceHash(1.0, 100);

	EXPECT_TRUE(spaceHash.isEmpty(Vector3df(5.0f, 5.0f, 5.0f)));

	spaceHash.add(Vector3df(5.2f, 5.0f, 5.0f));

	EXPECT_FALSE(spaceHash.isEmpty(Vector3df(5.0f, 5.0f, 5.0f)));
}

TEST(CompactSpaceHashTest, ToPosition_IsInverseOfToIndex)
{
	CompactSpaceHash spaceHash(2.0, 100);

	const std::array<int, 3> index{ 1, -2, 3 };
	const auto position = spaceHash.toPosition(index);
	const auto roundTripped = spaceHash.toIndex(position);

	EXPECT_EQ(index[0], roundTripped[0]);
	EXPECT_EQ(index[1], roundTripped[1]);
	EXPECT_EQ(index[2], roundTripped[2]);
}

TEST(CompactSpaceHashTest, Setup_ResetsCellSizeAndTableCapacity)
{
	CompactSpaceHash spaceHash(1.0, 4);
	spaceHash.add(Vector3df(0.0f, 0.0f, 0.0f));
	spaceHash.add(Vector3df(0.5f, 0.0f, 0.0f));

	spaceHash.setup(2.0, 100);
	spaceHash.add(Vector3df(0.0f, 0.0f, 0.0f));
	spaceHash.add(Vector3df(1.9f, 0.0f, 0.0f));

	const auto actual = spaceHash.findNeighborIndices(0);
	EXPECT_EQ(1u, actual.size());
}