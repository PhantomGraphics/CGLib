#include "gtest/gtest.h"

#include "../Core/Cylinder3d.h"
#include "PrimitiveTestSupport.h"

using namespace Phantom::Math;

template<typename T>
class Cylinder3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Cylinder3dTypedTest, PrimitiveScalars);

TYPED_TEST(Cylinder3dTypedTest, DefaultIsUnitCylinder)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cylinder3d<T> c;
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 0, 0), c.getPosition(T(0), T(0)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 0, 1), c.getPosition(T(0), T(1)), tol));
}

TYPED_TEST(Cylinder3dTypedTest, QuarterTurnsFollowUVecAndVVec)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cylinder3d<T> c(Vector3d<T>(1, 2, 3), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0), Vector3d<T>(0, 0, 4));
	EXPECT_TRUE(::areSame(Vector3d<T>(3, 2, 3), c.getPosition(T(0), T(0)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 5, 3), c.getPosition(T(0.25), T(0)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(-1, 2, 3), c.getPosition(T(0.5), T(0)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, -1, 3), c.getPosition(T(0.75), T(0)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(3, 2, 7), c.getPosition(T(0), T(1)), tol));
}

// Invariant: u is periodic, so u and u + 1 give the same point.
TYPED_TEST(Cylinder3dTypedTest, UIsPeriodic)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cylinder3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0), Vector3d<T>(0, 0, 4));
	for (const T u : { T(0), T(0.1), T(0.37), T(0.8) }) {
		EXPECT_TRUE(::areSame(c.getPosition(u, T(0.5)), c.getPosition(u + T(1), T(0.5)), tol * 10));
	}
}

// Invariant: v is linear along wvec, so the axis offset at v is v * wvec.
TYPED_TEST(Cylinder3dTypedTest, VMovesLinearlyAlongAxis)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Vector3d<T> w(0, 0, 4);
	const Cylinder3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0), w);
	for (const T v : { T(0), T(0.25), T(0.5), T(1) }) {
		const auto d = c.getPosition(T(0.3), v) - c.getPosition(T(0.3), T(0));
		EXPECT_TRUE(::areSame(v * w, d, tol));
	}
}

// Invariant: every sample is at distance |uvec| from the axis when uvec, vvec are
// orthogonal and equal length.
TYPED_TEST(Cylinder3dTypedTest, SamplesLieAtConstantRadius)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cylinder3d<T> c(Vector3d<T>(1, 1, 1), Vector3d<T>(0, 2, 0), Vector3d<T>(0, 0, 2), Vector3d<T>(5, 0, 0));
	for (int i = 0; i < 16; ++i) {
		const auto p = c.getPosition(T(i) / T(16), T(0.5)) - Vector3d<T>(1 + 2.5, 1, 1);
		EXPECT_NEAR(T(2), std::sqrt(p.y * p.y + p.z * p.z), tol);
		EXPECT_NEAR(T(0), p.x, tol);
	}
}

// Degenerate: a zero radial vector collapses the surface onto the axis (no NaN).
TYPED_TEST(Cylinder3dTypedTest, ZeroRadiusCollapsesToAxis)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cylinder3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(0, 0, 0), Vector3d<T>(0, 0, 0), Vector3d<T>(0, 0, 2));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 1), c.getPosition(T(0.3), T(0.5)), tol));
}

// Degenerate: a zero axis collapses the surface onto the base ring (no NaN).
TYPED_TEST(Cylinder3dTypedTest, ZeroHeightCollapsesToRing)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cylinder3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0), Vector3d<T>(0, 0, 0));
	EXPECT_TRUE(::areSame(c.getPosition(T(0.3), T(0)), c.getPosition(T(0.3), T(1)), tol));
}

// Negative radial vector mirrors the ring through the axis (u shifted by half a turn).
TYPED_TEST(Cylinder3dTypedTest, NegatedRadialVectorsAreHalfTurnShift)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cylinder3d<T> a(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0), Vector3d<T>(0, 0, 1));
	const Cylinder3d<T> b(Vector3d<T>(0, 0, 0), Vector3d<T>(-1, 0, 0), Vector3d<T>(0, -1, 0), Vector3d<T>(0, 0, 1));
	EXPECT_TRUE(::areSame(a.getPosition(T(0.5), T(0.5)), b.getPosition(T(0), T(0.5)), tol));
}
