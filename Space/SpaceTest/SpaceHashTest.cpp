#include "gtest/gtest.h"

#include "../Space/SpaceHash.h"

#include <algorithm>
#include <vector>

using namespace Phantom::Math;
using namespace Phantom::Space;

TEST(SpaceHashTest, TestGetNeighborIndices)
{
	SpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0, 0, 0));

	const auto actual = spaceHash.findNeighborIndices(Vector3df(0, 0, 0));
	EXPECT_EQ(1, actual.size());
}

TEST(SpaceHashTest, GetNeighbors_VerifyIndices)
{
	SpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0.0f, 0.0f, 0.0f));   // index 0
	spaceHash.add(Vector3df(0.5f, 0.0f, 0.0f));   // index 1
	spaceHash.add(Vector3df(2.0f, 0.0f, 0.0f));   // index 2 (too far)

	auto neighbors = spaceHash.findNeighborIndices(Vector3df(0.0f, 0.0f, 0.0f));
	std::vector<int> sorted(neighbors.begin(), neighbors.end());
	std::sort(sorted.begin(), sorted.end());

	ASSERT_EQ(2u, sorted.size());
	EXPECT_EQ(0, sorted[0]);
	EXPECT_EQ(1, sorted[1]);
}

TEST(SpaceHashTest, GetNeighbors_AcrossCellBoundary)
{
	SpaceHash spaceHash(1.0, 100);
	spaceHash.add(Vector3df(0.99f, 0.0f, 0.0f)); // index 0, cell x=0
	spaceHash.add(Vector3df(1.01f, 0.0f, 0.0f)); // index 1, cell x=1

	auto neighbors0 = spaceHash.findNeighborIndices(Vector3df(0.99f, 0.0f, 0.0f));
	std::vector<int> sorted0(neighbors0.begin(), neighbors0.end());
	std::sort(sorted0.begin(), sorted0.end());
	ASSERT_EQ(2u, sorted0.size());
	EXPECT_EQ(0, sorted0[0]);
	EXPECT_EQ(1, sorted0[1]);
}