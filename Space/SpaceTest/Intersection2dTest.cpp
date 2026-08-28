#include "pch.h"

#include "../Space/Intersection2d.h"
#include "CGLib/Math/Line2d.h"
#include "CGLib/Math/Circle2d.h"
#include "CGLib/Math/Vector2d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

TEST(Intersection2dTest, IntersectTwoPoints)
{
    Line2d<double> line({0.0, 0.0}, {4.0, 0.0});
    Circle2d<double> circle(1.0, {2.0, 0.0});
    double tolerance = 1.0e-8;

    auto ts = Intersection2d<double>::calculate(line, circle, tolerance);

    ASSERT_EQ(ts.size(), 2);
    Vector2d<double> p0 = line.getPosition(ts[0]);
    Vector2d<double> p1 = line.getPosition(ts[1]);
    EXPECT_NEAR(getLength(p0 - Vector2d<double>(1.0, 0.0)), 0.0, tolerance);
    EXPECT_NEAR(getLength(p1 - Vector2d<double>(3.0, 0.0)), 0.0, tolerance);
}

TEST(Intersection2dTest, Tangent)
{
    Line2d<double> line({0.0, 1.0}, {4.0, 1.0});
    Circle2d<double> circle(1.0, {2.0, 0.0});
    double tolerance = 1.0e-8;

    auto ts = Intersection2d<double>::calculate(line, circle, tolerance);

    ASSERT_EQ(ts.size(), 1);
    Vector2d<double> p = line.getPosition(ts[0]);
    EXPECT_NEAR(getLength(p - Vector2d<double>(2.0, 1.0)), 0.0, tolerance);
}

TEST(Intersection2dTest, NoIntersection)
{
    Line2d<double> line({0.0, 3.0}, {4.0, 3.0});
    Circle2d<double> circle(1.0, {2.0, 0.0});
    double tolerance = 1.0e-8;

    auto ts = Intersection2d<double>::calculate(line, circle, tolerance);

    EXPECT_TRUE(ts.empty());
}