#include "pch.h"

#include "../Numerics/SVD2d.h"

#include "CGLib/Math/pi.h"

using namespace Phantom::Math;
using namespace Phantom::Numerics;

namespace {
	constexpr auto tolerance = 1.0e-6;
}

TEST(SVD2dTest, TestCalculate)
{
	{
		Matrix2dd m(1, 2, 2, 3);
		SVD2d svd;
		const auto actual = svd.calculate(m);
		const auto values = actual.eigenValues;
		EXPECT_NEAR(-0.23606804013252258, values[0], tolerance);
		EXPECT_NEAR(4.2360687255859375, values[1], tolerance);
	}
	{
		Matrix2dd m = rotationMatrix(0.5 * PI);
		SVD2d svd;
		const auto actual = svd.calculate(m);
		const auto values = actual.eigenValues;
		EXPECT_NEAR(-1.0, values[0], tolerance);
		EXPECT_NEAR(1.0, values[1], tolerance);
	}
	{
		Matrix2dd m = rotationMatrix(PI);
		SVD2d svd;
		const auto actual = svd.calculate(m);
		const auto values = actual.eigenValues;
		EXPECT_NEAR(-1.0, values[0], tolerance);
		EXPECT_NEAR(-1.0, values[1], tolerance);
	}
}