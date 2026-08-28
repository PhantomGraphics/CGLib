#include "gtest/gtest.h"
#include "../Space/IntersectionCalculator.h"

#include "CGLib/Math/Ray3d.h"
#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Plane3d.h"
#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Box3d.h"
#include "CGLib/Math/Line3d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

namespace {
	const float tolerance = 1.0e-6f;

	Triangle3df getTriangle()
	{
		return Triangle3df({ Vector3dd(0,0,0), Vector3dd(10, 0, 0), Vector3dd(0, 10, 0) });
	}
}

TEST(IntersectionCalculatorTest, TestRayAndSphere)
{
	Ray3df ray(Vector3df(0,0,0), Vector3df(1,0,0));
	Sphere3df sphere(Vector3df(10, 0, 0), 1.0f);
	const auto found = IntersectionCalculator<float>::calculate(ray, sphere, tolerance);
	EXPECT_EQ(found.size(), 2);
	EXPECT_FLOAT_EQ(9.0f, found[0]);
	EXPECT_FLOAT_EQ(11.0f, found[1]);
}

TEST(IntersectionCalculatorTest, TestRayAndPlane)
{
	Ray3df ray(Vector3df(0, 0, 0), Vector3df(1, 0, 0));
	Plane3df plane(Vector3df(10, 0, 0), Vector3df(-1, 0, 0));
	const auto found = IntersectionCalculator<float>::calculate(ray, plane, tolerance);
	EXPECT_EQ(found.size(), 1);
	Vector3df expected(10, 0, 0);
	EXPECT_FLOAT_EQ(10.0f, found[0]);
}

TEST(IntersectionCalculatorTest, TestRayAndTriangle)
{
	const auto& triangle = getTriangle();

	{
		const Ray3df ray(Vector3df(1, 1, 1), Vector3df(0, 0, -1));
		const auto found = IntersectionCalculator<float>::calculate(ray, triangle, tolerance);
		EXPECT_EQ(1, found.size());
		EXPECT_FLOAT_EQ(1.0f, found[0]);
	}

	/*
	{
		const Ray3d ray(Vector3dd(10, 1, 1), Vector3dd(0, 0, -1));
		EXPECT_FALSE(algo.calculateIntersection(ray, triangle, tolerance));
	}
	*/
}

TEST(IntersectionCalculatorTest, TestRayAndBox)
{
	const Ray3df ray(Vector3dd(-5, 5, 5), Vector3dd(1, 0, 0));

	const Box3df box1(Vector3dd(0, 0, 0), Vector3dd(10, 10, 10));
	const auto i = IntersectionCalculator<float>::calculate(ray, box1, 1.0e-6f);
	EXPECT_EQ(2, i.size());
	EXPECT_FLOAT_EQ( 5.0f, i[0]);
	EXPECT_FLOAT_EQ(15.0f, i[1]);
//	EXPECT_TRUE(::areSame(Vector3df(10, 5, 5), algo.getIntersections()[1], tolerance));

//	const Box3df box2(Vector3df(50, 50, 50), Vector3df(100, 100, 100));
//	EXPECT_FALSE(algo.calculateIntersection(ray, box2, tolerance));
}

TEST(IntersectionCalculatorTest, TestLineAndSphere)
{
	Line3df line(Vector3df(0, 0, 0), Vector3df(10, 0, 0));
	Sphere3df sphere(Vector3df(10, 0, 0), 1.0f);
	const auto found = IntersectionCalculator<float>::calculate(line, sphere, tolerance);
	EXPECT_EQ(found.size(), 1);
	EXPECT_FLOAT_EQ(0.9f, found[0]);
	//EXPECT_FLOAT_EQ(11.0f, found[1]);
}

TEST(IntersectionCalculatorTest, TestSphereAndPlane)
{
	Sphere3df sphere(Vector3df(0, 0, 0), 5.0f);
	Plane3df plane(Vector3df(0, 0, 3), Vector3df(0, 0, 1));
	const auto found = IntersectionCalculator<float>::calculate(sphere, plane, tolerance);
	EXPECT_EQ(found.size(), 1);
	EXPECT_FLOAT_EQ(-3.0f, found[0]);
//	plane.getNormal()
}