#include "gtest/gtest.h"

#include "../Math/Cone3d.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-5f;
}

TEST(Cone3dTest, TestGetPosition)
{
	Cone3df c(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));

	// v = 0 -> base circle (full radius)
	EXPECT_TRUE(::areSame(Vector3df(1, 0, 0), c.getPosition(0.0f, 0.0f), tolerance));

	// v = 1 -> apex, independent of u
	EXPECT_TRUE(::areSame(c.getApex(), c.getPosition(0.0f, 1.0f), tolerance));
	EXPECT_TRUE(::areSame(c.getApex(), c.getPosition(0.25f, 1.0f), tolerance));
}

TEST(Cone3dTest, TestGetApex)
{
	const Cone3df c(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 2), c.getApex(), tolerance));
}
