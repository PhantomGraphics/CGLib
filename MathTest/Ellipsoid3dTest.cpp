#include "gtest/gtest.h"

#include "../Math/Ellipsoid3d.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-12f;
}

TEST(Ellipsoid3dTest, TestGetPosition)
{
	Ellipsoid3df e(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));
	const auto actual = e.getPosition(0, 0);
	const Vector3df expected(0, 0, 2);
	EXPECT_TRUE(::areSame(expected, actual, tolerance));
}