#include "gtest/gtest.h"

#include "../Space/LinearOctreeIndex.h"

using namespace Phantom::Space;

TEST(LinearOctreeIndexTest, ConstructFromIndex1d)
{
    LinearOctreeIndex idx(0u);
    EXPECT_EQ(0u, idx.getIndex1d());
}

TEST(LinearOctreeIndexTest, ConstructFromLevelAndNumber_Level0)
{
    // level=0: start = (8^0 - 1)/7 = 0, index1d = 0
    LinearOctreeIndex idx(0u, 0u);
    EXPECT_EQ(0u, idx.getIndex1d());
}

TEST(LinearOctreeIndexTest, ConstructFromLevelAndNumber_Level1)
{
    // level=1: start = (8^1 - 1)/7 = 1
    LinearOctreeIndex idx0(1u, 0u);
    EXPECT_EQ(1u, idx0.getIndex1d());

    LinearOctreeIndex idx7(1u, 7u);
    EXPECT_EQ(8u, idx7.getIndex1d());
}

TEST(LinearOctreeIndexTest, ConstructFromLevelAndNumber_Level2)
{
    // level=2: start = (8^2 - 1)/7 = 9
    LinearOctreeIndex idx(2u, 0u);
    EXPECT_EQ(9u, idx.getIndex1d());
}

TEST(LinearOctreeIndexTest, GetLevelAndNumber_Level0)
{
    LinearOctreeIndex idx(0u, 0u);
    const auto ln = idx.getLevelAndNumber();
    EXPECT_EQ(0u, ln.first);
    EXPECT_EQ(0u, ln.second);
}

TEST(LinearOctreeIndexTest, GetLevelAndNumber_Level1)
{
    LinearOctreeIndex idx(1u, 3u);
    const auto ln = idx.getLevelAndNumber();
    EXPECT_EQ(1u, ln.first);
    EXPECT_EQ(3u, ln.second);
}

TEST(LinearOctreeIndexTest, GetLevelAndNumber_Level2)
{
    LinearOctreeIndex idx(2u, 5u);
    const auto ln = idx.getLevelAndNumber();
    EXPECT_EQ(2u, ln.first);
    EXPECT_EQ(5u, ln.second);
}

TEST(LinearOctreeIndexTest, GetParentIndex_Level1ToLevel0)
{
    LinearOctreeIndex child(1u, 4u);
    const auto parent = child.getParentIndex();
    EXPECT_EQ(LinearOctreeIndex(0u, 0u), parent);
}

TEST(LinearOctreeIndexTest, GetParentIndex_Level2ToLevel1)
{
    // number=8, parentNumber = 8>>3 = 1
    LinearOctreeIndex child(2u, 8u);
    const auto parent = child.getParentIndex();
    EXPECT_EQ(LinearOctreeIndex(1u, 1u), parent);
}

TEST(LinearOctreeIndexTest, ComparisonOperators)
{
    LinearOctreeIndex a(1u, 0u); // index1d = 1
    LinearOctreeIndex b(1u, 1u); // index1d = 2

    EXPECT_TRUE(a == a);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}
