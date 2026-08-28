#include "pch.h"

#include "../Numerics/SVD3d.h"

#include "CGLib/Math/pi.h"

using namespace Phantom::Math;
using namespace Phantom::Numerics;

namespace {
	constexpr auto tolerance = 1.0e-6;
}

TEST(SVD3dTest, TestCalculate)
{
	{
		Matrix3dd m = rotationMatrixX(0.5 * PI);
		SVD3d svd;
		const auto actual = svd.calculate(m);
		const auto values = actual.eigenValues;
		EXPECT_NEAR(-1.0, values[0], tolerance);
		EXPECT_NEAR(1.0, values[1], tolerance);
		EXPECT_NEAR(1.0, values[2], tolerance);
	}
	{
		Matrix3dd m = rotationMatrixX(PI);
		SVD3d svd;
		const auto actual = svd.calculate(m);
		const auto values = actual.eigenValues;
		EXPECT_NEAR(-1.0, values[0], tolerance);
		EXPECT_NEAR(-1.0, values[1], tolerance);
		EXPECT_NEAR(1.0, values[2], tolerance);
	}
}

TEST(SVD3dTest, TestCalculateFullJacobiReconstructsInput)
{
	// A non-symmetric matrix -- U/S/V must satisfy A == U * diag(S) * V^T.
	Matrix3dd m(
		2.0, 0.0, 1.0,
		0.0, 3.0, 0.0,
		1.0, 0.0, 2.0);

	SVD3d svd;
	const auto actual = svd.calculateFullJacobi(m);
	ASSERT_TRUE(actual.isOk);

	Matrix3dd s(0.0);
	s[0][0] = actual.singularValues[0];
	s[1][1] = actual.singularValues[1];
	s[2][2] = actual.singularValues[2];

	const Matrix3dd reconstructed = actual.matrixU * s * glm::transpose(actual.matrixV);
	EXPECT_TRUE(areSame(m, reconstructed, tolerance));

	// Singular values are non-negative and descending.
	EXPECT_GE(actual.singularValues[0], actual.singularValues[1]);
	EXPECT_GE(actual.singularValues[1], actual.singularValues[2]);
	EXPECT_GE(actual.singularValues[2], 0.0);
}

TEST(SVD3dTest, TestCalculateFullJacobiOnRotationMatrix)
{
	// For an orthogonal input, U*V^T recovers the matrix itself (all singular values == 1).
	Matrix3dd m = rotationMatrixZ(0.3);
	SVD3d svd;
	const auto actual = svd.calculateFullJacobi(m);
	ASSERT_TRUE(actual.isOk);

	EXPECT_NEAR(1.0, actual.singularValues[0], tolerance);
	EXPECT_NEAR(1.0, actual.singularValues[1], tolerance);
	EXPECT_NEAR(1.0, actual.singularValues[2], tolerance);

	const Matrix3dd recomposedRotation = actual.matrixU * glm::transpose(actual.matrixV);
	EXPECT_TRUE(areSame(m, recomposedRotation, tolerance));
}