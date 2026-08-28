#include "gtest/gtest.h"
#include "../Math/Ray3d.h"

using namespace Phantom::Math;

namespace {
	float tolerance = 1.0e-12f;
}

TEST(Ray3dTest, TestGetPosition)
{
	const Ray3df ray(Vector3df(0, 0, 0), Vector3df(1, 0, 0));
	const auto& actual = ray.getPosition(5.0);
	const Vector3df expected(5.0, 0.0, 0.0);
	EXPECT_TRUE(areSame(expected, actual, tolerance));
}