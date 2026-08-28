#include "gtest/gtest.h"
#include "../Math/Circle3d.h"
#include "../Math/Vector3d.h"

using namespace Phantom::Math;

TEST(Circle3dTest, DefaultConstructor)
{
    Circle3df c;
    EXPECT_FLOAT_EQ(c.getRadius(), 0.5f);
    EXPECT_FLOAT_EQ(c.getCenter().x, 0.0f);
    EXPECT_FLOAT_EQ(c.getCenter().y, 0.0f);
    EXPECT_FLOAT_EQ(c.getCenter().z, 0.0f);
    EXPECT_FLOAT_EQ(c.getNormal().x, 0.0f);
    EXPECT_FLOAT_EQ(c.getNormal().y, 0.0f);
    EXPECT_FLOAT_EQ(c.getNormal().z, 1.0f);
}

TEST(Circle3dTest, ParameterizedConstructor)
{
    Vector3df center(1.5f, -2.5f, 3.0f);
    Vector3df normal(0.0f, 1.0f, 0.0f);
    float radius = 2.0f;
    Circle3df c(radius, center, normal);
    EXPECT_FLOAT_EQ(c.getRadius(), radius);
    EXPECT_FLOAT_EQ(c.getCenter().x, center.x);
    EXPECT_FLOAT_EQ(c.getCenter().y, center.y);
    EXPECT_FLOAT_EQ(c.getCenter().z, center.z);
    EXPECT_FLOAT_EQ(c.getNormal().x, normal.x);
    EXPECT_FLOAT_EQ(c.getNormal().y, normal.y);
    EXPECT_FLOAT_EQ(c.getNormal().z, normal.z);
}

TEST(Circle3dTest, DoubleType)
{
    Vector3dd center(2.0, 4.0, -1.0);
    Vector3dd normal(1.0, 0.0, 0.0);
    double radius = 5.5;
    Circle3dd c(radius, center, normal);
    EXPECT_DOUBLE_EQ(c.getRadius(), radius);
    EXPECT_DOUBLE_EQ(c.getCenter().x, center.x);
    EXPECT_DOUBLE_EQ(c.getCenter().y, center.y);
    EXPECT_DOUBLE_EQ(c.getCenter().z, center.z);
    EXPECT_DOUBLE_EQ(c.getNormal().x, normal.x);
    EXPECT_DOUBLE_EQ(c.getNormal().y, normal.y);
    EXPECT_DOUBLE_EQ(c.getNormal().z, normal.z);
}