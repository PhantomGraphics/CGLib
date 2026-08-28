#include "gtest/gtest.h"

#include "../Math/Vector2d.h"

using namespace Phantom::Math;

TEST(Vector2dTest, TestGetLengthSquared)
{
	const Vector2df v(4, 3);

	const auto actual = getLengthSquared(v);
	EXPECT_FLOAT_EQ(25.0f, actual);
}

TEST(Vector2dTest, TestGetLength)
{
	const Vector2df v(4,3);

	const auto actual = getLength(v);
	EXPECT_FLOAT_EQ(5.0f, actual);
}

TEST(Vector2dTest, TestGetDistanceSquared)
{
	const Vector2df v1(1, 1);
	const Vector2df v2(5, 4);

	const auto actual = getDistanceSquared(v1, v2);
	EXPECT_FLOAT_EQ(25, actual);
}

TEST(Vector2dTest, TestGetDistance)
{
	const Vector2df v1(1,1);
	const Vector2df v2(5,4);

	const auto actual = getDistance(v1, v2);
	EXPECT_FLOAT_EQ(5.0f, actual);
}

TEST(Vector2dTest, TestNormalize)
{
	const Vector2df v(3.0f, 4.0f);
	const auto normalized = glm::normalize(v);

	EXPECT_FLOAT_EQ(1.0f, getLength(normalized));
	EXPECT_FLOAT_EQ(0.6f, normalized.x);
	EXPECT_FLOAT_EQ(0.8f, normalized.y);
}

TEST(Vector2dTest, TestDotProduct)
{
	const Vector2df xAxis(1.0f, 0.0f);
	const Vector2df yAxis(0.0f, 1.0f);

	EXPECT_FLOAT_EQ(0.0f, glm::dot(xAxis, yAxis));
	EXPECT_FLOAT_EQ(1.0f, glm::dot(xAxis, xAxis));
}

TEST(Vector2dTest, TestAddSubtract)
{
	const Vector2df lhs(1.0f, 2.0f);
	const Vector2df rhs(3.0f, 4.0f);

	EXPECT_EQ(Vector2df(4.0f, 6.0f), lhs + rhs);
	EXPECT_EQ(Vector2df(-2.0f, -2.0f), lhs - rhs);
}