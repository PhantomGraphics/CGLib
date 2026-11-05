#include "gtest/gtest.h"
#include "../Noise/DomainWarp.h"
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

TEST(FractalSettings, RidgedFeedback) {
    auto constant=[](float)->std::optional<float>{return .5f;};
    EXPECT_FLOAT_EQ(*evaluateFractal(constant,{FractalMode::RidgedMultifractal,2,2,.5f,1,2}),.3125f/1.5f);
    EXPECT_FALSE(validateFractalSettings({FractalMode::RidgedMultifractal,2,2,.5f,0,2}));
}

TEST(DomainWarp, ReproducibleAndPreservesHeight) {
    DomainWarp a(42),b(42);
    const Phantom::Math::Vector3df p(.3f,7,.6f);
    const auto x=a.sample2D(p),y=b.sample2D(p);
    ASSERT_TRUE(x); ASSERT_TRUE(y); EXPECT_EQ(*x,*y); EXPECT_FLOAT_EQ(x->y,p.y);
    DomainWarpSettings s; s.strength=0;
    EXPECT_EQ(*a.sample3D(p,s),p);
    s.scale=0; EXPECT_FALSE(a.sample3D(p,s));
}

TEST(FractalSettings, FeedbackLimitsAndFailures) {
    auto sample=[](float)->std::optional<float>{return .5f;};
    EXPECT_FLOAT_EQ(*evaluateFractal(sample,{FractalMode::RidgedMultifractal,1}),.25f);
    EXPECT_FLOAT_EQ(*evaluateFractal(sample,{FractalMode::RidgedMultifractal,3,2,0}),.25f);
    EXPECT_FLOAT_EQ(*evaluateFractal(sample,{FractalMode::RidgedMultifractal,2,2,.5f,1,0}),.25f/1.5f);
    EXPECT_FALSE(evaluateFractal(sample,{FractalMode::RidgedMultifractal,2,2,.5f,std::numeric_limits<float>::max(),2}));
    EXPECT_TRUE(validateFractalSettings({FractalMode::FBm,2,2,.5f,-1,-1}));
}
TEST(DomainWarp, BoundsAndIndependentAxisSeeds) {
    DomainWarp warp(42), different(43);
    const Phantom::Math::Vector3df p(.31f,2,.47f);
    DomainWarpSettings s; s.octaves=1;
    const auto a=warp.sample3D(p,s),b=different.sample3D(p,s);
    ASSERT_TRUE(a); ASSERT_TRUE(b); EXPECT_NE(*a,*b);
    EXPECT_NE(a->x-p.x,a->y-p.y);
    EXPECT_FALSE(warp.sample3D({std::numeric_limits<float>::infinity(),0,0}));
    EXPECT_FALSE(warp.sample2D({std::numeric_limits<float>::max(),0,0}));
    s.strength=0; EXPECT_EQ(*warp.sample2D({std::numeric_limits<float>::max(),0,0},s),Phantom::Math::Vector3df(std::numeric_limits<float>::max(),0,0));
    s.gain=-1; EXPECT_FALSE(warp.sample3D(p,s));
}

TEST(DomainWarp, ChannelSeedContractIsPinned) {
    DomainWarp warp(42); DomainWarpSettings settings; settings.octaves=1;
    const Phantom::Math::Vector3df p(.31f,.57f,.73f);
    const auto result=warp.sample3D(p,settings); ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(result->x,p.x+*PerlinNoise(551831576u).sample3D(p.x,p.y,p.z));
    EXPECT_FLOAT_EQ(result->y,p.y+*PerlinNoise(144025891u).sample3D(p.x,p.y,p.z));
    EXPECT_FLOAT_EQ(result->z,p.z+*PerlinNoise(322543647u).sample3D(p.x,p.y,p.z));
}
