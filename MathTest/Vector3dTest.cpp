#include "gtest/gtest.h"

#include "../Math/Vector3d.h"

using namespace Phantom::Math;

TEST(Vector3dTest, TestGetLengthSquared)
{
	const Vector3df v(1, 2, 3);

	const auto actual = getLengthSquared(v);
	EXPECT_FLOAT_EQ(14, actual);
}

TEST(Vector3dTest, TestGetLength)
{
	const Vector3df v(1, 2, 3);

	const auto actual = getLength(v);
	EXPECT_FLOAT_EQ(std::sqrt(14.0f), actual);
}

TEST(Vector3dTest, TestGetDistanceSquared)
{
	const Vector3df v1(0, 0, 0);
	const Vector3df v2(1, 2, 3);

	const auto actual = getDistanceSquared(v1, v2);
	EXPECT_FLOAT_EQ(14, actual);
}

TEST(Vector3dTest, TestGetDistance)
{
	const Vector3df v1(0, 0, 0);
	const Vector3df v2(1, 2, 3);

	const auto actual = getDistance(v1, v2);
	EXPECT_FLOAT_EQ(std::sqrt(14.0f), actual);
}

TEST(Vector3dTest, TestNormalize)
{
	const Vector3df v(2.0f, 3.0f, 6.0f);
	const auto normalized = glm::normalize(v);

	EXPECT_FLOAT_EQ(1.0f, getLength(normalized));
}

TEST(Vector3dTest, TestDotProduct)
{
	const Vector3df xAxis(1.0f, 0.0f, 0.0f);
	const Vector3df yAxis(0.0f, 1.0f, 0.0f);

	EXPECT_FLOAT_EQ(0.0f, glm::dot(xAxis, yAxis));
	EXPECT_FLOAT_EQ(1.0f, glm::dot(xAxis, xAxis));
}

TEST(Vector3dTest, TestCrossProduct)
{
	const Vector3df xAxis(1.0f, 0.0f, 0.0f);
	const Vector3df yAxis(0.0f, 1.0f, 0.0f);

	EXPECT_EQ(Vector3df(0.0f, 0.0f, 1.0f), glm::cross(xAxis, yAxis));
}

TEST(Vector3dTest, TestAddSubtract)
{
	const Vector3df lhs(1.0f, 2.0f, 3.0f);
	const Vector3df rhs(4.0f, 5.0f, 6.0f);

	EXPECT_EQ(Vector3df(5.0f, 7.0f, 9.0f), lhs + rhs);
	EXPECT_EQ(Vector3df(-3.0f, -3.0f, -3.0f), lhs - rhs);
}