#include "gtest/gtest.h"

#include <cmath>

#include "../Math/Statistics.h"

using namespace Phantom::Math;

TEST(StatisticsTest, TestGetAverage)
{
	Statistics<float> stat({ 5.0f, 8.0f, 9.0f, 7.0f });
	EXPECT_FLOAT_EQ(7.25, stat.getAverage());
}

TEST(StatisticsTest, TestGetVariance)
{
	Statistics<float> stat({ 5.0f, 8.0f, 9.0f, 7.0f });
	EXPECT_FLOAT_EQ(2.1875, stat.getVariance());
}

TEST(StatisticsTest, TestGetStandardDeviation)
{
	Statistics<float> stat({ 5.0f, 8.0f, 9.0f, 7.0f });
	EXPECT_FLOAT_EQ(std::sqrtf(2.1875f), stat.getStandardDeviation());
}

TEST(StatisticsTest, TestSingleValue)
{
	Statistics<float> stat({ 7.0f });

	EXPECT_FLOAT_EQ(7.0f, stat.getAverage());
	EXPECT_FLOAT_EQ(0.0f, stat.getVariance());
	EXPECT_FLOAT_EQ(0.0f, stat.getStandardDeviation());
}