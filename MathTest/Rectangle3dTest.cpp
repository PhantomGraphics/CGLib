#include "gtest/gtest.h"

#include "../Math/Rectangle3d.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-12f;
}

TEST(Rectangle3dTest, TestGetPosition)
{
	const Rectangle3df quad(Vector3df(0, 0, 0), Vector3df(10, 0, 0), Vector3df(0, 10, 0));
	EXPECT_TRUE(areSame(Vector3df(5, 0, 0), quad.getPosition(0.5, 0.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df(0, 5, 0), quad.getPosition(0.0, 0.5), tolerance));
}

TEST(Quad3dTest, TestGetNormal)
{
	const Rectangle3df quad(Vector3df(0, 0, 0), Vector3df(10, 0, 0), Vector3df(0, 10, 0));
	EXPECT_TRUE(areSame(Vector3df(0, 0, 1), quad.getNormal(), tolerance));
}