#include "gtest/gtest.h"

#include "../Core/Triangle3d.h"

#include "PrimitiveTestSupport.h"

#include <limits>

using namespace Phantom::Math;

TEST(Triangle3dTest, TestGetNormal)
{
	const Triangle3df triangle
	(
		{ 
			Vector3df(0, 0, 0),
			Vector3df(1, 0, 0),
			Vector3df(0, 1, 0)
		}
	);
	EXPECT_EQ(Vector3df(0, 0, 1), triangle.getNormal());
}

TEST(Triangle3dTest, TestGetArea)
{
	const Triangle3df triangle
	(
		{
		Vector3df(0, 0, 0),
		Vector3df(1, 0, 0),
		Vector3df(0, 1, 0)
		}
	);
	EXPECT_FLOAT_EQ(0.5f, triangle.getArea());
}

template<typename T>
class Triangle3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Triangle3dTypedTest, PrimitiveScalars);

TYPED_TEST(Triangle3dTypedTest, TryGetNormalMatchesGetNormalForValidTriangle)
{
	using T = TypeParam;
	const Triangle3d<T> t({ Vector3d<T>(0, 0, 0), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0) });
	Vector3d<T> n(9, 9, 9);
	ASSERT_TRUE(t.tryGetNormal(n));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 1), n, primitiveTolerance<T>()));
	EXPECT_EQ(t.getNormal(), n);
}

TYPED_TEST(Triangle3dTypedTest, TryGetNormalFailsForDegenerateTriangleAndKeepsOut)
{
	using T = TypeParam;
	const Vector3d<T> p(1, 1, 1);
	const Triangle3d<T> coincident({ p, p, p });
	const Triangle3d<T> collinear({ Vector3d<T>(0, 0, 0), Vector3d<T>(1, 1, 1), Vector3d<T>(3, 3, 3) });
	const Triangle3d<T> nonFinite({ Vector3d<T>(0, 0, 0), Vector3d<T>(std::numeric_limits<T>::quiet_NaN(), 0, 0), Vector3d<T>(0, 1, 0) });
	for (const auto& t : { coincident, collinear, nonFinite }) {
		const Vector3d<T> sentinel(7, 8, 9);
		Vector3d<T> n = sentinel;
		EXPECT_FALSE(t.tryGetNormal(n));
		EXPECT_EQ(sentinel, n);
	}
}

// Phase 4 follow-up: Space now uses tryGetNormal(), so getNormal() of a degenerate
// triangle is the zero vector instead of NaN (previously Space detected NaN).
TYPED_TEST(Triangle3dTypedTest, GetNormalOfDegenerateTriangleIsZero)
{
	using T = TypeParam;
	const Vector3d<T> p(1, 1, 1);
	EXPECT_EQ(Vector3d<T>(0, 0, 0), Triangle3d<T>({ p, p, p }).getNormal());
	const Triangle3d<T> collinear({ Vector3d<T>(0, 0, 0), Vector3d<T>(1, 1, 1), Vector3d<T>(3, 3, 3) });
	EXPECT_EQ(Vector3d<T>(0, 0, 0), collinear.getNormal());
}
