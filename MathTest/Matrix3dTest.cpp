#include "gtest/gtest.h"
#include "../Math/Matrix3d.h"
#include "../Math/pi.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-6f;
	const float pi = static_cast<float>(PI);
}

TEST(Matrix3dTest, TestIdentity)
{
	const Matrix3df expected
	(
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
	);

	EXPECT_TRUE(areSame(expected, rotationMatrixX(0.0f), tolerance));
}

TEST(Matrix3dTest, TestRotationMatrixX)
{

	{
		const auto actual = rotationMatrixX(0.0f);
		const Matrix3df expected
		(
			1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			0.0, 0.0, 1.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}

	{
		const auto actual = rotationMatrixX(0.5f * pi);
		const Matrix3df expected
		(
			1.0, 0.0, 0.0,
			0.0, 0.0,-1.0,
			0.0, 1.0, 0.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}

	{
		const auto actual = rotationMatrixX(pi);
		const Matrix3df expected
		(
			1.0, 0.0, 0.0,
			0.0,-1.0, 0.0,
			0.0, 0.0,-1.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}
}

TEST(Matrix3dTest, TestRotationMatrixY)
{
	{
		const auto actual = rotationMatrixY(0.0f);
		const Matrix3df expected
		(
			1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			0.0, 0.0, 1.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}

	{
		const auto actual = rotationMatrixY(0.5f * pi);
		const Matrix3df expected
		(
			 0.0, 0.0, 1.0,
			 0.0, 1.0, 0.0,
			-1.0, 0.0, 0.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}

	{
		const auto actual = rotationMatrixY(pi);
		const Matrix3df expected
		(
			-1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			0.0, 0.0, -1.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}
}

TEST(Matrix3dTest, TestRotationMatrixZ)
{
	{
		const auto actual = rotationMatrixZ(0.0f);
		const Matrix3df expected
		(
			1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			0.0, 0.0, 1.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}

	{
		const auto actual = rotationMatrixZ(0.5f * pi);
		const Matrix3df expected
		(
			0.0,-1.0, 0.0,
			1.0, 0.0, 0.0,
			0.0, 0.0, 1.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}

	{
		const auto actual = rotationMatrixZ(pi);
		const Matrix3df expected
		(
			-1.0, 0.0, 0.0,
			 0.0,-1.0, 0.0,
			 0.0, 0.0, 1.0
		);
		EXPECT_TRUE(areSame(expected, actual, tolerance));
	}
}

TEST(Matrix3dTest, TestMultiplication)
{
	const auto rotation = rotationMatrixY(0.5f * pi);
	const auto inverse = rotationMatrixY(-0.5f * pi);
	const auto actual = rotation * inverse;
	const Matrix3df expected
	(
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
	);

	EXPECT_TRUE(areSame(expected, actual, tolerance));
}

TEST(Matrix3dTest, TestDeterminant)
{
	const auto actual = glm::determinant(rotationMatrixZ(0.5f * pi));
	EXPECT_NEAR(1.0f, actual, tolerance);
}