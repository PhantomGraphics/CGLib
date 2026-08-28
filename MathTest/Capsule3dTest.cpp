#include "gtest/gtest.h"

#include "../Math/Capsule3d.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-5f;
}

TEST(Capsule3dTest, TestGetPosition)
{
	// radius = 1 (|uvec|), axis along +Z with cylindrical length 2.
	Capsule3df c(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));

	// v = 0 -> bottom pole (tip of the bottom hemisphere), independent of u.
	EXPECT_TRUE(::areSame(Vector3df(0, 0, -1), c.getPosition(0.0f, 0.0f), tolerance));

	// v = 1 -> top pole (tip of the top hemisphere), independent of u.
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 3), c.getPosition(0.0f, 1.0f), tolerance));

	// v = 1/3 -> equator ring where the bottom cap meets the cylindrical side.
	EXPECT_TRUE(::areSame(Vector3df(1, 0, 0), c.getPosition(0.0f, 1.0f / 3.0f), tolerance));

	// v = 2/3 -> equator ring where the cylindrical side meets the top cap.
	EXPECT_TRUE(::areSame(Vector3df(1, 0, 2), c.getPosition(0.0f, 2.0f / 3.0f), tolerance));

	// v = 0.5 -> midpoint of the cylindrical side.
	EXPECT_TRUE(::areSame(Vector3df(1, 0, 1), c.getPosition(0.0f, 0.5f), tolerance));
}

TEST(Capsule3dTest, TestAccessors)
{
	const Capsule3df c(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 0), c.getBottom(), tolerance));
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 2), c.getTop(), tolerance));
	EXPECT_FLOAT_EQ(1.0f, c.getRadius());
}
