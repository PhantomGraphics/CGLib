#include "gtest/gtest.h"

#include "../Math/Box3d.h"

using namespace Phantom::Math;

namespace {
	const double tolerance = 1.0e-12;
}

TEST(Box3dTest, TestGetMin)
{
	const Box3dd box1;
	const Box3dd box2(Vector3dd(1, 2, 3), Vector3dd(4, 5, 6));
	EXPECT_EQ(Vector3dd(0, 0, 0), box1.getMin());
	EXPECT_EQ(Vector3dd(1, 2, 3), box2.getMin());
}

TEST(Box3dTest, TestGetMax)
{
	EXPECT_EQ(Vector3dd(1, 1, 1), Box3dd().getMax());
	EXPECT_EQ(Vector3dd(4, 5, 6), Box3dd(Vector3dd(1, 2, 3), Vector3dd(4, 5, 6)).getMax());
}

TEST(Box3dTest, TestGetLength)
{
	Box3dd box(Vector3dd(1.0f, 1.0f, 1.0f), Vector3dd(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(Vector3dd(0.0, 1.0, 2.0), box.getLength());
}

TEST(Box3dTest, TestAdd)
{
	Box3dd b1(Vector3dd(0, 0, 0), Vector3dd(2, 2, 2));
	const Box3dd b2(Vector3dd(-1, 1, 1), Vector3dd(3, 3, 3));
	b1.add(b2);
	const Box3dd expected(Vector3dd(-1, 0, 0), Vector3dd(3, 3, 3));
	EXPECT_TRUE(expected.isSame(b1, tolerance));
}

TEST(Box3dTest, TestGetPosition)
{
	const Box3dd b1(Vector3dd(0, 0, 0), Vector3dd(2, 2, 2));
	EXPECT_EQ(Vector3dd(0, 0, 0), b1.getPosition(0, 0, 0));
	EXPECT_EQ(Vector3dd(2, 0, 0), b1.getPosition(1, 0, 0));
	EXPECT_EQ(Vector3dd(2, 2, 0), b1.getPosition(1, 1, 0));
	EXPECT_EQ(Vector3dd(0, 2, 0), b1.getPosition(0, 1, 0));
}

TEST(Box3dTest, TestIsInside)
{
	const Box3dd b1(Vector3dd(0, 0, 0), Vector3dd(2, 2, 2));
	EXPECT_TRUE(b1.contains(Vector3dd(1, 1, 1), 0.0));
	EXPECT_FALSE(b1.contains(Vector3dd(0, 1, 1), 0.0));
	EXPECT_FALSE(b1.contains(Vector3dd(5, 1, 1), 0.0));
}

TEST(Box3dTest, TestContains)
{
	const Box3dd outer(Vector3dd(0, 0, 0), Vector3dd(4, 4, 4));
	const Box3dd inner(Vector3dd(1, 1, 1), Vector3dd(3, 3, 3));
	const Box3dd touching(Vector3dd(0, 1, 1), Vector3dd(2, 2, 2));

	EXPECT_TRUE(outer.contains(inner, tolerance));
	EXPECT_FALSE(outer.contains(touching, 0.0));
}

TEST(Box3dTest, TestIsSame)
{
	const Box3dd expected(Vector3dd(0, 0, 0), Vector3dd(2, 2, 2));
	const Box3dd almostSame(Vector3dd(0.0, 0.0, 0.0), Vector3dd(2.0, 2.0, 2.0 + 1.0e-13));

	EXPECT_TRUE(expected.isSame(almostSame, tolerance));
	EXPECT_FALSE(expected.isSame(Box3dd(Vector3dd(0, 0, 0), Vector3dd(2, 2, 2.1)), tolerance));
}