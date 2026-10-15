#include "gtest/gtest.h"

#include "../Math/Box2d.h"
#include "../Math/Circle2d.h"
#include "../Math/Circle3d.h"
#include "../Math/Line2d.h"
#include "../Math/Line3d.h"
#include "../Math/Plane3d.h"
#include "../Math/Ray3d.h"
#include "PrimitiveTestSupport.h"

#include <limits>

using namespace Phantom::Math;

// float / double parity for the older primitives whose original suites only cover float.
// (Gaussian is a non-template float-only class, so it has no double variant to test.)

template<typename T>
class PrimitiveParityTest : public ::testing::Test {};
TYPED_TEST_SUITE(PrimitiveParityTest, PrimitiveScalars);

TYPED_TEST(PrimitiveParityTest, Box2dConstructorOrdersCorners)
{
	using T = TypeParam;
	const Box2d<T> b(Vector2d<T>(3, -1), Vector2d<T>(-2, 4));
	EXPECT_EQ(Vector2d<T>(-2, -1), b.getMin());
	EXPECT_EQ(Vector2d<T>(3, 4), b.getMax());
}

// add(point) grows the box to the union of box and point; adding an inside point is a no-op.
TYPED_TEST(PrimitiveParityTest, Box2dAddPointGrowsOnlyWhenOutside)
{
	using T = TypeParam;
	Box2d<T> b(Vector2d<T>(0, 0), Vector2d<T>(2, 2));
	b.add(Vector2d<T>(1, 1));
	EXPECT_EQ(Vector2d<T>(0, 0), b.getMin());
	EXPECT_EQ(Vector2d<T>(2, 2), b.getMax());
	b.add(Vector2d<T>(-1, 5));
	EXPECT_EQ(Vector2d<T>(-1, 0), b.getMin());
	EXPECT_EQ(Vector2d<T>(2, 5), b.getMax());
}

// The degenerate box is the identity for add(): the first point becomes both min and max.
TYPED_TEST(PrimitiveParityTest, Box2dDegenerateBoxIsAddIdentity)
{
	using T = TypeParam;
	auto b = Box2d<T>::createDegeneratedBox();
	b.add(Vector2d<T>(3, 4));
	EXPECT_EQ(Vector2d<T>(3, 4), b.getMin());
	EXPECT_EQ(Vector2d<T>(3, 4), b.getMax());
	auto c = Box2d<T>::createDegeneratedBox();
	c.add(Box2d<T>(Vector2d<T>(1, 2), Vector2d<T>(3, 4)));
	EXPECT_EQ(Vector2d<T>(1, 2), c.getMin());
	EXPECT_EQ(Vector2d<T>(3, 4), c.getMax());
}

TYPED_TEST(PrimitiveParityTest, Box2dAddBoxIsUnion)
{
	using T = TypeParam;
	Box2d<T> a(Vector2d<T>(0, 0), Vector2d<T>(1, 1));
	a.add(Box2d<T>(Vector2d<T>(2, -1), Vector2d<T>(3, 0.5)));
	EXPECT_EQ(Vector2d<T>(0, -1), a.getMin());
	EXPECT_EQ(Vector2d<T>(3, 1), a.getMax());
}

TYPED_TEST(PrimitiveParityTest, Circle2dSamplesAtConstantRadiusAndPeriodic)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Circle2d<T> c(T(2), Vector2d<T>(1, -1));
	for (int i = 0; i < 12; ++i) {
		const T u = T(i) / T(12);
		const auto p = c.getPosition(u) - c.getCenter();
		EXPECT_NEAR(T(2), std::sqrt(p.x * p.x + p.y * p.y), tol);
		EXPECT_TRUE(::areSame(c.getPosition(u), c.getPosition(u + T(1)), tol * 10));
	}
	EXPECT_TRUE(::areSame(Vector2d<T>(3, -1), c.getPosition(T(0)), tol));
	EXPECT_TRUE(::areSame(Vector2d<T>(1, 1), c.getPosition(T(0.25)), tol));
}

TYPED_TEST(PrimitiveParityTest, Circle2dZeroRadiusCollapsesToCenter)
{
	using T = TypeParam;
	const Circle2d<T> c(T(0), Vector2d<T>(1, 2));
	EXPECT_TRUE(::areSame(Vector2d<T>(1, 2), c.getPosition(T(0.37)), primitiveTolerance<T>()));
}

TYPED_TEST(PrimitiveParityTest, Circle3dDefaultsAndAccessors)
{
	using T = TypeParam;
	const Circle3d<T> d;
	EXPECT_EQ(T(0.5), d.getRadius());
	EXPECT_EQ(Vector3d<T>(0, 0, 0), d.getCenter());
	EXPECT_EQ(Vector3d<T>(0, 0, 1), d.getNormal());
	const Circle3d<T> c(T(2), Vector3d<T>(1, 2, 3), Vector3d<T>(0, 1, 0));
	EXPECT_EQ(T(2), c.getRadius());
	EXPECT_EQ(Vector3d<T>(1, 2, 3), c.getCenter());
	EXPECT_EQ(Vector3d<T>(0, 1, 0), c.getNormal());
}

TYPED_TEST(PrimitiveParityTest, LineEndpointsLengthAndDirection)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Line3d<T> l(Vector3d<T>(1, 2, 3), Vector3d<T>(4, 6, 3));
	EXPECT_TRUE(::areSame(l.getStart(), l.getPosition(T(0)), tol));
	EXPECT_TRUE(::areSame(l.getEnd(), l.getPosition(T(1)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(2.5, 4, 3), l.getPosition(T(0.5)), tol));
	EXPECT_NEAR(T(5), l.getLength(), tol);
	EXPECT_EQ(Vector3d<T>(3, 4, 0), l.getDirection());
	const Line2d<T> m(Vector2d<T>(0, 0), Vector2d<T>(3, 4));
	EXPECT_NEAR(T(5), m.getLength(), tol);
	EXPECT_TRUE(::areSame(Vector2d<T>(1.5, 2), m.getPosition(T(0.5)), tol));
}

// A zero-length line has zero length and every parameter maps to the start point.
TYPED_TEST(PrimitiveParityTest, LineZeroLengthCollapsesToStart)
{
	using T = TypeParam;
	const Line3d<T> l(Vector3d<T>(1, 2, 3), Vector3d<T>(1, 2, 3));
	EXPECT_EQ(T(0), l.getLength());
	EXPECT_EQ(Vector3d<T>(1, 2, 3), l.getPosition(T(0.7)));
}

// Signed distance: zero on the plane, linear along the normal, antisymmetric under normal flip.
TYPED_TEST(PrimitiveParityTest, PlaneSignedDistanceInvariants)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Plane3d<T> p(Vector3d<T>(0, 0, 1), Vector3d<T>(0, 0, 1));
	const Plane3d<T> flipped(Vector3d<T>(0, 0, 1), Vector3d<T>(0, 0, -1));
	EXPECT_NEAR(T(0), p.getDistance(Vector3d<T>(5, -3, 1)), tol);
	EXPECT_NEAR(T(2), p.getDistance(Vector3d<T>(5, -3, 3)), tol);
	EXPECT_NEAR(T(-3), p.getDistance(Vector3d<T>(0, 0, -2)), tol);
	for (const Vector3d<T> q : { Vector3d<T>(1, 2, 3), Vector3d<T>(-4, 0, 7) }) {
		EXPECT_NEAR(p.getDistance(q), -flipped.getDistance(q), tol);
	}
}

TYPED_TEST(PrimitiveParityTest, Ray3dPositionIsLinearInParameter)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ray3d<T> r(Vector3d<T>(1, 1, 1), Vector3d<T>(0, 2, 0));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 1, 1), r.getPosition(0.0), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 5, 1), r.getPosition(2.0), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, -1, 1), r.getPosition(-1.0), tol));
}
