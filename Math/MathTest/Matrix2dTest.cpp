#include "gtest/gtest.h"
#include "../Math/Matrix2d.h"
#include "../Math/pi.h"

using namespace Phantom::Math;

namespace {
	const auto tolerance = 1.0e-6f;
	const auto pi = static_cast<float>(PI);
}

TEST(Matrix2dTest, TestIdentity)
{
	const Matrix2df expected
	(
		1.0, 0.0,
		0.0, 1.0
	);

	EXPECT_TRUE(areSame(expected, rotationMatrix(0.0f), tolerance));
}

TEST(Matrix2dTest, TestRotationMatrix)
{
	const Matrix2df expected
	(
		0.0, -1.0,
		1.0, 0.0
	);
	const auto actual = rotationMatrix(0.5f * pi);
	EXPECT_TRUE(areSame(expected, actual, tolerance));
}

TEST(Matrix2dTest, TestMultiplication)
{
	const auto rotation = rotationMatrix(0.5f * pi);
	const auto inverse = rotationMatrix(-0.5f * pi);
	const auto actual = rotation * inverse;
	const Matrix2df expected
	(
		1.0, 0.0,
		0.0, 1.0
	);

	EXPECT_TRUE(areSame(expected, actual, tolerance));
}

TEST(Matrix2dTest, TestDeterminant)
{
	const auto actual = glm::determinant(rotationMatrix(0.5f * pi));
	EXPECT_NEAR(1.0f, actual, tolerance);
}