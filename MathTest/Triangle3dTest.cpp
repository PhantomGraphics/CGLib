#include "gtest/gtest.h"

#include "../Math/Triangle3d.h"

using namespace Phantom::Math;

TEST(Triangle3dTest, TestGetNormal)
{
	const Triangle3df triangle
	(
		{ 
			Vector3df(0, 0, 0),
			Vector3df(1, 0, 0),
			Vector3df(0, 1, 0)
		}
	);
	EXPECT_EQ(Vector3df(0, 0, 1), triangle.getNormal());
}

TEST(Triangle3dTest, TestGetArea)
{
	const Triangle3df triangle
	(
		{
		Vector3df(0, 0, 0),
		Vector3df(1, 0, 0),
		Vector3df(0, 1, 0)
		}
	);
	EXPECT_FLOAT_EQ(0.5f, triangle.getArea());
}
