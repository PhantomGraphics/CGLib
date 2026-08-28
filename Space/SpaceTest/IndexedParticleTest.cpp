#include "gtest/gtest.h"

#include "../Space/IndexedParticle.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

TEST(IndexedParticleTest, DefaultConstructor)
{
    IndexedParticle p;
    EXPECT_EQ(0, p.getGridId());
    EXPECT_EQ(0, p.getId());
}

TEST(IndexedParticleTest, ConstructWithPosition)
{
    const Vector3df pos(1.0f, 2.0f, 3.0f);
    IndexedParticle p(pos);
    EXPECT_EQ(pos.x, p.getPosition().x);
    EXPECT_EQ(pos.y, p.getPosition().y);
    EXPECT_EQ(pos.z, p.getPosition().z);
}

TEST(IndexedParticleTest, SetAndGetId)
{
    IndexedParticle p;
    p.setId(42);
    EXPECT_EQ(42, p.getId());
}

TEST(IndexedParticleTest, ToIndex)
{
    const Vector3df pos(1.0f, 2.0f, 3.0f);
    const auto idx = IndexedParticle::toIndex(pos, 1.0f);
    EXPECT_EQ(1, idx[0]);
    EXPECT_EQ(2, idx[1]);
    EXPECT_EQ(3, idx[2]);
}

TEST(IndexedParticleTest, ToIndexWithLargerCellSize)
{
    const Vector3df pos(3.0f, 6.0f, 9.0f);
    const auto idx = IndexedParticle::toIndex(pos, 3.0f);
    EXPECT_EQ(1, idx[0]);
    EXPECT_EQ(2, idx[1]);
    EXPECT_EQ(3, idx[2]);
}

TEST(IndexedParticleTest, ToGridId)
{
    // toGridId = (iz<<20) + (iy<<10) + ix for index {1,2,3}
    const Vector3df pos(1.0f, 2.0f, 3.0f);
    const auto id = IndexedParticle::toGridId(pos, 1.0f);
    const auto expected = (3 << 20) + (2 << 10) + 1;
    EXPECT_EQ(expected, id);
}

TEST(IndexedParticleTest, SetGridId)
{
    IndexedParticle p(Vector3df(1.0f, 2.0f, 3.0f));
    p.setGridId(1.0f);
    const auto expected = IndexedParticle::toGridId(Vector3df(1.0f, 2.0f, 3.0f), 1.0f);
    EXPECT_EQ(expected, p.getGridId());
}

TEST(IndexedParticleTest, LessThanOperator)
{
    IndexedParticle a(Vector3df(0.0f, 0.0f, 0.0f)); // toIndex={0,0,0}, gridId=0
    IndexedParticle b(Vector3df(1.0f, 1.0f, 1.0f)); // toIndex={1,1,1}, gridId=(1<<20)+(1<<10)+1
    a.setGridId(1.0f);
    b.setGridId(1.0f);
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}
