#include "gtest/gtest.h"

#include "../Math/Sphere3d.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-12f;
}

TEST(Sphere3dTest, TestGetPosition)
{
	const Sphere3df s(Vector3df(0.0, 0.0, 0.0), 1.0);

	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getPosition(0.0, 0.0, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getPosition(0.0, 0.5, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getPosition(0.0, 1.0, 1.0), tolerance));

	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getPosition(0.0, 0.5, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df(-1, 0, 0), s.getPosition(0.5, 0.5, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getPosition(1.0, 0.5, 1.0), tolerance));

	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getPosition(1.0, 0.0, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getPosition(1.0, 0.5, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getPosition(1.0, 1.0, 1.0), tolerance));
}

TEST(Sphere3dTest, TestGetNormal)
{
	const Sphere3df s(Vector3df(0.0, 0.0, 0.0), 1.0);

	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getNormal(0.0, 0.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 1, 0, 0), s.getNormal(0.5, 0.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getNormal(1.0, 0.0), tolerance));
}

TEST(Sphere3dTest, TestContains)
{
	const Sphere3df s(Vector3df(0.0f, 0.0f, 0.0f), 1.0f);

	EXPECT_TRUE(s.contains(Vector3df(0.5f, 0.0f, 0.0f), tolerance));
	EXPECT_FALSE(s.contains(Vector3df(2.0f, 0.0f, 0.0f), tolerance));
	EXPECT_TRUE(s.contains(Vector3df(1.0f, 0.0f, 0.0f), 0.01f));
}