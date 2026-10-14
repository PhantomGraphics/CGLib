#include "pch.h"

#include "../Graphics/ColorTable.h"
#include <gtest/gtest.h>

using namespace Phantom::Graphics;

static void ExpectColorNear(const ColorRGBAf& a, const ColorRGBAf& b, float tol = 1e-5f)
{
	EXPECT_NEAR(a.r, b.r, tol);
	EXPECT_NEAR(a.g, b.g, tol);
	EXPECT_NEAR(a.b, b.b, tol);
	EXPECT_NEAR(a.a, b.a, tol);
}

TEST(ColorTableTest, ResolutionAndSetGet)
{
	ColorTable table(4);
	ASSERT_EQ(table.getResolution(), 4);

	ColorRGBAf c1(0.1f, 0.2f, 0.3f, 0.4f);
	table.setColor(1, c1);

	const auto got = table.getColor(1);
	ExpectColorNear(got, c1);
}

TEST(ColorTableTest, GetColorOutOfRangeReturnsLast)
{
	ColorTable table(3);
	ASSERT_EQ(table.getResolution(), 3);

	ColorRGBAf c0(0.0f, 0.0f, 0.0f, 1.0f);
	ColorRGBAf c1(0.5f, 0.5f, 0.5f, 1.0f);
	ColorRGBAf c2(1.0f, 1.0f, 1.0f, 1.0f);

	table.setColor(0, c0);
	table.setColor(1, c1);
	table.setColor(2, c2);

	// 範囲内
	ExpectColorNear(table.getColor(0), c0);
	ExpectColorNear(table.getColor(1), c1);
	ExpectColorNear(table.getColor(2), c2);

	// 範囲外アクセスは最後の要素を返す
	ExpectColorNear(table.getColor(10), c2);
}

TEST(ColorTableTest, CreateJetTableBasic)
{
	const int resolution = 5;
	ColorTable jet = ColorTable::createJetTable(resolution);
	ASSERT_EQ(jet.getResolution(), resolution);

	// resolution = 5 のときの期待値（作成ロジックに基づく）
	// i=0 -> v=0.0 -> approx (0.0, 0.0, 0.5, 1.0)
	// i=2 -> v=0.5 -> approx (0.5, 1.0, 0.5, 1.0)
	// i=4 -> v=1.0 -> approx (0.5, 0.0, 0.0, 1.0)
	const ColorRGBAf expected0(0.0f, 0.0f, 0.5f, 1.0f);
	const ColorRGBAf expected2(0.5f, 1.0f, 0.5f, 1.0f);
	const ColorRGBAf expected4(0.5f, 0.0f, 0.0f, 1.0f);

	ExpectColorNear(jet.getColor(0), expected0);
	ExpectColorNear(jet.getColor(2), expected2);
	ExpectColorNear(jet.getColor(4), expected4);
}

