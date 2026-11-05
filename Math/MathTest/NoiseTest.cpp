#include "gtest/gtest.h"
#include "../Noise/PerlinNoise.h"
#include <limits>
using namespace Phantom::Math::Noise;
TEST(PerlinNoise, ReproducibleAndContinuous) {
    PerlinNoise a(42), b(42), c(17);
    EXPECT_EQ(a.sample2D(-0.37f,0.61f),b.sample2D(-0.37f,0.61f));
    EXPECT_NE(a.sample2D(-0.37f,0.61f),c.sample2D(-0.37f,0.61f));
    ASSERT_TRUE(a.sample3D(.2f,.3f,.4f));
    EXPECT_EQ(a.sample3D(.2f,.3f,.4f), b.sample3D(.2f,.3f,.4f));
    EXPECT_FLOAT_EQ(*a.sample3D(0,0,0),0);
    EXPECT_NEAR(*a.sample2D(-.00001f,.37f),*a.sample2D(.00001f,.37f),.0001f);
    EXPECT_NEAR(*a.sample3D(.37f,-.00001f,.42f),*a.sample3D(.37f,.00001f,.42f),.0001f);
    EXPECT_FALSE(a.sample2D(std::numeric_limits<float>::infinity(),0));
    EXPECT_FALSE(a.sample3D(0,0,std::numeric_limits<float>::max()));
}
TEST(FractalSettings, FormulaAndValidation) {
    auto constant=[](float)->std::optional<float> {return -.25f;};
    EXPECT_FLOAT_EQ(*evaluateFractal(constant),-.25f);
    EXPECT_FLOAT_EQ(*evaluateFractal(constant,{FractalMode::Turbulence}),.25f);
    EXPECT_FLOAT_EQ(*evaluateFractal(constant,{FractalMode::Ridged}),.5625f);
    EXPECT_FLOAT_EQ(*evaluateFractal([](float f)->std::optional<float>{return f;},{FractalMode::FBm,2,2,.5f}),4.0f/3);
    EXPECT_FLOAT_EQ(*evaluateFractal(constant,{FractalMode::FBm,12,2,0}),-.25f);
    EXPECT_FALSE(evaluateFractal(constant,{FractalMode::FBm,0}));
    EXPECT_FALSE(evaluateFractal(constant,{FractalMode::FBm,13}));
    EXPECT_FALSE(evaluateFractal(constant,{FractalMode::FBm,1,1}));
    EXPECT_FALSE(evaluateFractal([](float)->std::optional<float>{return std::nullopt;}));
}
