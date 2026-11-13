#include "gtest/gtest.h"
#include "../Fractal/EscapeTime.h"
#include <limits>
using namespace Phantom::Math::Fractal;
TEST(EscapeTime, KnownPoints) {
    const auto inside = mandelbrot(0, 0);
    ASSERT_TRUE(inside);
    EXPECT_FALSE(inside->escaped);
    EXPECT_EQ(inside->iterations, 256u);
    const auto outside = mandelbrot(3, 0);
    ASSERT_TRUE(outside);
    EXPECT_TRUE(outside->escaped);
    EXPECT_EQ(outside->iterations, 1u);
    ASSERT_TRUE(julia(0, 0, 0, 0));
    EXPECT_FALSE(julia(0, 0, 0, 0)->escaped);
    const auto huge = julia(1e300, 1e300);
    ASSERT_TRUE(huge);
    EXPECT_TRUE(huge->escaped);
    EXPECT_EQ(huge->iterations, 1u);
}
TEST(EscapeTime, InvalidAndCancelled) {
    EXPECT_FALSE(mandelbrot(0, 0, 0));
    EXPECT_FALSE(mandelbrot(0, 0, 4097));
    EXPECT_FALSE(mandelbrot(std::numeric_limits<double>::infinity(), 0));
    EXPECT_FALSE(evaluate(0, 0, 0, 0, 256, [] { return false; }));
}
