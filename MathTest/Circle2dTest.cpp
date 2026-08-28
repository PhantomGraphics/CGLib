#include "gtest/gtest.h"

#include "../Math/Circle2d.h"

using namespace Phantom::Math;

TEST(Circle2dTest, DefaultConstructor)
{
    Circle2df c;
    EXPECT_FLOAT_EQ(c.getRadius(), 0.5f);
    EXPECT_FLOAT_EQ(c.getCenter().x, 0.0f);
    EXPECT_FLOAT_EQ(c.getCenter().y, 0.0f);
}

TEST(Circle2dTest, ParameterizedConstructor)
{
    Vector2d<float> center(1.5f, -2.5f);
    float radius = 3.0f;
    Circle2df c(radius, center);
    EXPECT_FLOAT_EQ(c.getRadius(), radius);
    EXPECT_FLOAT_EQ(c.getCenter().x, center.x);
    EXPECT_FLOAT_EQ(c.getCenter().y, center.y);
}

TEST(Circle2dTest, DoubleType)
{
    Vector2d<double> center(2.0, 4.0);
    double radius = 5.5;
    Circle2dd c(radius, center);
    EXPECT_DOUBLE_EQ(c.getRadius(), radius);
    EXPECT_DOUBLE_EQ(c.getCenter().x, center.x);
    EXPECT_DOUBLE_EQ(c.getCenter().y, center.y);
}

TEST(Circle2dTest, GetPosition)
{
    Vector2d<float> center(0.0f, 0.0f);
    float radius = 1.0f;
    const Circle2df c(radius, center);
    Vector2d<float> pos0 = c.getPosition(0.0f);
    EXPECT_NEAR(pos0.x, 1.0f, 1.0e-6);
    EXPECT_NEAR(pos0.y, 0.0f, 1.0e-6);
    Vector2d<float> pos90 = c.getPosition(0.25);
    EXPECT_NEAR(pos90.x, 0.0f, 1.0e-6);
    EXPECT_NEAR(pos90.y, 1.0f, 1.0e-6);
    Vector2d<float> pos180 = c.getPosition(0.5f);
    EXPECT_NEAR(pos180.x, -1.0f, 1.0e-6);
    EXPECT_NEAR(pos180.y, 0.0f, 1.0e-6);
    Vector2d<float> pos270 = c.getPosition(0.75f);
    EXPECT_NEAR(pos270.x, 0.0f, 1.0e-6);
    EXPECT_NEAR(pos270.y, -1.0f, 1.0e-6);
}