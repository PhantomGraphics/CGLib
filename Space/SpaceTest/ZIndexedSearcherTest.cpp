#include "gtest/gtest.h"

#include "../Space/ZIndexedSearcher.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

TEST(ZIndexedSearcherTest, ToIndex)
{
    ZIndexedSearcher searcher(1.0f, Vector3df(0.0f, 0.0f, 0.0f));
    const auto idx = searcher.toIndex(Vector3df(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(1u, idx[0]);
    EXPECT_EQ(1u, idx[1]);
    EXPECT_EQ(1u, idx[2]);
}

TEST(ZIndexedSearcherTest, ToIndexOffset)
{
    ZIndexedSearcher searcher(1.0f, Vector3df(0.0f, 0.0f, 0.0f));
    const auto idx = searcher.toIndex(Vector3df(1.5f, 2.5f, 3.5f));
    EXPECT_EQ(2u, idx[0]);
    EXPECT_EQ(3u, idx[1]);
    EXPECT_EQ(4u, idx[2]);
}

TEST(ZIndexedSearcherTest, FindNeighborsSingleParticle)
{
    ZIndexedSearcher searcher(1.0f, Vector3df(0.0f, 0.0f, 0.0f));
    searcher.add(Vector3df(0.0f, 0.0f, 0.0f));
    searcher.sort();

    const auto neighbors = searcher.findNeighbors(Vector3df(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(1u, neighbors.size());
    EXPECT_EQ(0, neighbors.front());
}

TEST(ZIndexedSearcherTest, FindNeighborsDistantParticle)
{
    ZIndexedSearcher searcher(1.0f, Vector3df(0.0f, 0.0f, 0.0f));
    searcher.add(Vector3df(10.0f, 10.0f, 10.0f));
    searcher.sort();

    const auto neighbors = searcher.findNeighbors(Vector3df(0.0f, 0.0f, 0.0f));
    EXPECT_TRUE(neighbors.empty());
}

TEST(ZIndexedSearcherTest, FindNeighborsMultipleParticles)
{
    ZIndexedSearcher searcher(1.0f, Vector3df(0.0f, 0.0f, 0.0f));
    searcher.add(Vector3df(0.0f, 0.0f, 0.0f)); // index 0
    searcher.add(Vector3df(0.5f, 0.0f, 0.0f)); // index 1 (near)
    searcher.add(Vector3df(9.0f, 9.0f, 9.0f)); // index 2 (far)
    searcher.sort();

    const auto neighbors = searcher.findNeighbors(Vector3df(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(2u, neighbors.size());
}
