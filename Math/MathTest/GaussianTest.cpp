#include "gtest/gtest.h"

#include "../Math/Gaussian.h"

using namespace Phantom::Math;

TEST(GaussianTest, TestGet)
{
	Gaussian g(1.0f, 0.0f, 1.0f);
	const auto w = g.getWeight(0.0);
	EXPECT_NEAR(1, w, 1.0e-12);
  //EXPECT_TRUE(true);
}

TEST(GaussianTest, TestCreateNormalDistributionFunc)
{
	const auto g = Gaussian::createNormalDistributionFunc(0.0, 1.0);
	const auto w = g.getWeight(0.0);
	EXPECT_NEAR(0.3984, w, 1.0e-3);
	//EXPECT_TRUE(true);
}

TEST(GaussianTest, TestSymmetry)
{
	const auto g = Gaussian::createNormalDistributionFunc(2.0f, 3.0f);

	EXPECT_NEAR(g.getWeight(2.0f + 1.0f), g.getWeight(2.0f - 1.0f), 1.0e-6f);
	EXPECT_NEAR(g.getWeight(2.0f + 2.5f), g.getWeight(2.0f - 2.5f), 1.0e-6f);
}

TEST(GaussianTest, TestPeak)
{
	const auto g = Gaussian::createNormalDistributionFunc(0.0f, 1.0f);

	EXPECT_GT(g.getWeight(0.0f), g.getWeight(1.0f));
	EXPECT_GT(g.getWeight(0.0f), g.getWeight(-1.0f));
}