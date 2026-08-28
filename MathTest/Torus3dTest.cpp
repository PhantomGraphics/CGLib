#include "gtest/gtest.h"

#include "../Math/Torus3d.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-5f;
}

TEST(Torus3dTest, TestGetPosition)
{
	const Torus3df t(Vector3df(0, 0, 0), Vector3df(0, 1, 0), 1.0f, 0.25f);

	// u = 0, v = 0 -> outer equator point of the tube closest to +Z
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 1.25f), t.getPosition(0.0f, 0.0f), tolerance));

	// u = 0, v = 0.25 -> top of the tube (+Y) above the +Z ring point
	EXPECT_TRUE(::areSame(Vector3df(0, 0.25f, 1.0f), t.getPosition(0.0f, 0.25f), tolerance));

	// u = 0.25, v = 0 -> outer equator point of the tube closest to +X
	EXPECT_TRUE(::areSame(Vector3df(1.25f, 0, 0), t.getPosition(0.25f, 0.0f), tolerance));
}

TEST(Torus3dTest, TestAccessors)
{
	const Torus3df t(Vector3df(1, 2, 3), Vector3df(0, 1, 0), 2.0f, 0.5f);
	EXPECT_TRUE(::areSame(Vector3df(1, 2, 3), t.getCenter(), tolerance));
	EXPECT_FLOAT_EQ(2.0f, t.getMajorRadius());
	EXPECT_FLOAT_EQ(0.5f, t.getMinorRadius());
}
