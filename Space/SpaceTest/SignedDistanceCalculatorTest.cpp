#include "gtest/gtest.h"
#include "../Space/SignedDistanceCalculator.h"

#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Plane3d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

TEST(SignedDistanceCalculatorTest, TestCalclateSphere)
{
	SignedDistanceCalculator<float> c;
	Sphere3df sphere(Vector3df(0, 0, 0), 10.0f);
	const auto d1 = c.calculate(Vector3df(11, 0, 0), sphere);
	EXPECT_FLOAT_EQ(d1, 1.0f);

	const auto d2 = c.calculate(Vector3df(9, 0, 0), sphere);
	EXPECT_FLOAT_EQ(d2, -1.0f);

	const auto d3 = c.calculate(Vector3df(10, 0, 0), sphere);
	EXPECT_FLOAT_EQ(d3, 0.0f);
}

TEST(SignedDistanceCalculatorTest, TestCalculatePlane)
{
	SignedDistanceCalculator<float> c;
	Plane3df plane(Vector3df(0, 0, 0), Vector3df(1, 0, 0));
	const auto d1 = c.calculate(Vector3df(1, 0, 0), plane);
	EXPECT_FLOAT_EQ(d1, 1.0f);

	const auto d2 = c.calculate(Vector3df(-1, 0, 0), plane);
	EXPECT_FLOAT_EQ(d2, -1.0f);

	const auto d3 = c.calculate(Vector3df(0, 0, 0), plane);
	EXPECT_FLOAT_EQ(d3, 0.0f);
}