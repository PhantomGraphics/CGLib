#include "gtest/gtest.h"

#include <cmath>

#include "../Math/Statistics.h"
#include "PrimitiveTestSupport.h"

#include <limits>

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
	EXPECT_FLOAT_EQ(std::sqrt(2.1875f), stat.getStandardDeviation());
}

TEST(StatisticsTest, TestSingleValue)
{
	Statistics<float> stat({ 7.0f });

	EXPECT_FLOAT_EQ(7.0f, stat.getAverage());
	EXPECT_FLOAT_EQ(0.0f, stat.getVariance());
	EXPECT_FLOAT_EQ(0.0f, stat.getStandardDeviation());
}

template<typename T>
class StatisticsTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(StatisticsTypedTest, PrimitiveScalars);

TYPED_TEST(StatisticsTypedTest, TryGetMatchesLegacyResults)
{
	using T = TypeParam;
	const Statistics<T> stat({ T(5), T(8), T(9), T(7) });
	T avg = 0, var = 0, sd = 0;
	ASSERT_TRUE(stat.tryGetAverage(avg));
	ASSERT_TRUE(stat.tryGetVariance(var));
	ASSERT_TRUE(stat.tryGetStandardDeviation(sd));
	EXPECT_EQ(stat.getAverage(), avg);
	EXPECT_EQ(stat.getVariance(), var);
	EXPECT_EQ(stat.getStandardDeviation(), sd);
	EXPECT_NEAR(T(7.25), avg, primitiveTolerance<T>());
	EXPECT_NEAR(T(2.1875), var, primitiveTolerance<T>());   // population variance (divide by N)
}

TYPED_TEST(StatisticsTypedTest, TryGetFailsOnEmptyAndKeepsOut)
{
	using T = TypeParam;
	const Statistics<T> stat;
	T a = T(42), v = T(43), s = T(44);
	EXPECT_FALSE(stat.tryGetAverage(a));
	EXPECT_FALSE(stat.tryGetVariance(v));
	EXPECT_FALSE(stat.tryGetStandardDeviation(s));
	EXPECT_EQ(T(42), a);
	EXPECT_EQ(T(43), v);
	EXPECT_EQ(T(44), s);
}

TYPED_TEST(StatisticsTypedTest, TryGetFailsOnNonFiniteValues)
{
	using T = TypeParam;
	for (const T bad : { std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::infinity(),
		-std::numeric_limits<T>::infinity() }) {
		const Statistics<T> stat({ T(1), bad, T(3) });
		T a = T(-1), v = T(-1), s = T(-1);
		EXPECT_FALSE(stat.tryGetAverage(a));
		EXPECT_FALSE(stat.tryGetVariance(v));
		EXPECT_FALSE(stat.tryGetStandardDeviation(s));
		EXPECT_EQ(T(-1), a);
		EXPECT_EQ(T(-1), v);
		EXPECT_EQ(T(-1), s);
	}
}

// Finite inputs whose sum overflows give a non-finite result -> failure, not inf.
TYPED_TEST(StatisticsTypedTest, TryGetFailsWhenSumOverflows)
{
	using T = TypeParam;
	const T big = std::numeric_limits<T>::max();
	const Statistics<T> stat({ big, big });
	T a = T(-1);
	EXPECT_FALSE(stat.tryGetAverage(a));
	EXPECT_EQ(T(-1), a);
}

TYPED_TEST(StatisticsTypedTest, SingleValueHasZeroVariance)
{
	using T = TypeParam;
	const Statistics<T> stat({ T(7) });
	T v = T(-1), s = T(-1);
	ASSERT_TRUE(stat.tryGetVariance(v));
	ASSERT_TRUE(stat.tryGetStandardDeviation(s));
	EXPECT_EQ(T(0), v);
	EXPECT_EQ(T(0), s);
}
