#include "gtest/gtest.h"
#include "../Space/DistanceCalculator.h"

#include "CGLib/Math/Ray3d.h"
#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Triangle3d.h"
#include "CGLib/Math/Vector3d.h"

using namespace Phantom::Math;
using namespace Phantom::Space;

TEST(DistanceCalculatorTest, TestRayAndSphere)
{
	DistanceCalculator<float> c;
	Ray3df ray(Vector3df(0, 0, 0), Vector3df(1, 0, 0));
	Sphere3df sphere(Vector3df(10, 0, 0), 1.0f);
	const auto d = c.calculate(ray, sphere, 1.0e-9f);
	EXPECT_EQ(d.size(), 2);
	EXPECT_FLOAT_EQ(d[0],  9.0f);
	EXPECT_FLOAT_EQ(d[1], 11.0f);
}

TEST(DistanceCalculatorTest, TestTrianglePoint_Vertex)
{
	const Triangle3df tri({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
	const auto p = Vector3df(0,0,0);
	const auto d = DistanceCalculator<float>::calculate(tri, p, 1.0e-6f);
	EXPECT_FLOAT_EQ(d, 0.0f);
}

TEST(DistanceCalculatorTest, TestTrianglePoint_Edge)
{
	const Triangle3df tri({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
	const auto p = Vector3df(0.5f, 0.0f, 0.0f); // AB 上
	const auto d = DistanceCalculator<float>::calculate(tri, p, 1.0e-6f);
	EXPECT_FLOAT_EQ(d, 0.0f);
}

TEST(DistanceCalculatorTest, TestTrianglePoint_Interior)
{
	const Triangle3df tri({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
	const auto p = Vector3df(0.2f, 0.2f, 0.0f); // 三角形内部
	const auto d = DistanceCalculator<float>::calculate(tri, p, 1.0e-6f);
	EXPECT_FLOAT_EQ(d, 0.0f);
}

TEST(DistanceCalculatorTest, TestTrianglePoint_AbovePlane)
{
	const Triangle3df tri({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
	const auto p = Vector3df(0.2f, 0.2f, 1.0f); // 法線方向に距離1
	const auto d = DistanceCalculator<float>::calculate(tri, p, 1.0e-6f);
	EXPECT_NEAR(d, 1.0f, 1.0e-6f);
}

TEST(DistanceCalculatorTest, TestTrianglePoint_Tolerance)
{
	const Triangle3df tri({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
	// 面から非常に近い点（距離 < tolerance） -> 0 を返す
	const auto p = Vector3df(0.2f, 0.2f, 1.0e-8f);
	const float tol = 1.0e-6f;
	const auto d = DistanceCalculator<float>::calculate(tri, p, tol);
	EXPECT_FLOAT_EQ(d, 0.0f);
}