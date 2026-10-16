#include "gtest/gtest.h"

#include <limits>

#include "../Core/Ellipse3d.h"
#include "PrimitiveTestSupport.h"

using namespace Phantom::Math;

template<typename T>
class Ellipse3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Ellipse3dTypedTest, PrimitiveScalars);

TYPED_TEST(Ellipse3dTypedTest, DefaultIsUnitCircleInXY)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e;
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 0, 0), e.getPosition(T(0)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 1, 0), e.getPosition(T(0.25)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(-1, 0, 0), e.getPosition(T(0.5)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, -1, 0), e.getPosition(T(0.75)), tol));
}

TYPED_TEST(Ellipse3dTypedTest, CurveFollowsCenterAndAxes)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(1, 2, 3), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 0, 3));
	EXPECT_TRUE(::areSame(Vector3d<T>(3, 2, 3), e.getPosition(T(0)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 2, 6), e.getPosition(T(0.25)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(-1, 2, 3), e.getPosition(T(0.5)), tol));
}

// Invariant: the curve is periodic in u.
TYPED_TEST(Ellipse3dTypedTest, CurveIsPeriodic)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Ellipse3d<T> e(Vector3d<T>(0, 0, 0), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0));
	for (const T u : { T(0), T(0.13), T(0.5), T(0.91) }) {
		EXPECT_TRUE(::areSame(e.getPosition(u), e.getPosition(u + T(1)), tol));
	}
}

// Invariant: the filled surface at v = 1 is the curve, at v = 0 the center,
// and v scales the radial offset linearly.
TYPED_TEST(Ellipse3dTypedTest, SurfaceScalesRadiallyWithV)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(1, 1, 1), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0));
	for (const T u : { T(0), T(0.2), T(0.6) }) {
		EXPECT_TRUE(::areSame(e.getCenter(), e.getPosition(u, T(0)), tol));
		EXPECT_TRUE(::areSame(e.getPosition(u), e.getPosition(u, T(1)), tol));
		const auto half = e.getPosition(u, T(0.5)) - e.getCenter();
		const auto full = e.getPosition(u) - e.getCenter();
		EXPECT_TRUE(::areSame(full * T(0.5), half, tol));
	}
}

// Invariant: samples satisfy (x/a)^2 + (y/b)^2 = 1 for axis-aligned axes.
TYPED_TEST(Ellipse3dTypedTest, SamplesSatisfyEllipseEquation)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(0, 0, 0), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0));
	for (int i = 0; i < 16; ++i) {
		const auto p = e.getPosition(T(i) / T(16));
		EXPECT_NEAR(T(1), (p.x / 2) * (p.x / 2) + (p.y / 3) * (p.y / 3), tol);
		EXPECT_NEAR(T(0), p.z, tol);
	}
}

// Zero axes collapse the ellipse to its center (no NaN).
TYPED_TEST(Ellipse3dTypedTest, ZeroAxesCollapseToCenter)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(1, 2, 3), Vector3d<T>(0, 0, 0), Vector3d<T>(0, 0, 0));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 2, 3), e.getPosition(T(0.3)), tol));
}

// Phase 0 baseline was the unnormalized cross (0,0,6); Phase 2 made it a unit normal.
TYPED_TEST(Ellipse3dTypedTest, GetNormalIsNormalizedCross)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(0, 0, 0), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 1), e.getNormal(), tol));
}

// A valid ellipse yields a unit normal perpendicular to both axes.
TYPED_TEST(Ellipse3dTypedTest, GetNormalIsUnitAndPerpendicular)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(0, 0, 0), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0));
	const auto n = e.getNormal();
	EXPECT_NEAR(T(1), getLength(n), tol);
	EXPECT_NEAR(T(0), glm::dot(n, e.getUVec()), tol);
	EXPECT_NEAR(T(0), glm::dot(n, e.getVVec()), tol);
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 1), n, tol));
}

// Orientation follows cross(uvec, vvec): swapping the axes flips the normal.
TYPED_TEST(Ellipse3dTypedTest, GetNormalFlipsWhenAxesSwap)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(0, 0, 0), Vector3d<T>(0, 3, 0), Vector3d<T>(2, 0, 0));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, -1), e.getNormal(), tol));
}

TYPED_TEST(Ellipse3dTypedTest, TryGetNormalSucceedsForValidEllipse)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(5, 5, 5), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 0, 3));
	Vector3d<T> n(9, 9, 9);
	ASSERT_TRUE(e.tryGetNormal(n));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, -1, 0), n, tol));
}

// Failure must leave `out` untouched; getNormal() then returns the zero vector.
TYPED_TEST(Ellipse3dTypedTest, TryGetNormalFailsForDegenerateAxes)
{
	using T = TypeParam;
	const T nan = std::numeric_limits<T>::quiet_NaN();
	const T inf = std::numeric_limits<T>::infinity();
	const Vector3d<T> zero(0, 0, 0);
	const Vector3d<T> x(1, 0, 0);
	const Ellipse3d<T> cases[] = {
		Ellipse3d<T>(zero, zero, x),                              // zero uvec
		Ellipse3d<T>(zero, x, zero),                              // zero vvec
		Ellipse3d<T>(zero, x, Vector3d<T>(-3, 0, 0)),             // antiparallel
		Ellipse3d<T>(zero, x, Vector3d<T>(2, 0, 0)),              // parallel
		Ellipse3d<T>(zero, Vector3d<T>(nan, 0, 0), Vector3d<T>(0, 1, 0)),
		Ellipse3d<T>(zero, Vector3d<T>(inf, 0, 0), Vector3d<T>(0, 1, 0)),
	};
	for (const auto& e : cases) {
		const Vector3d<T> sentinel(7, 8, 9);
		Vector3d<T> n = sentinel;
		EXPECT_FALSE(e.tryGetNormal(n));
		EXPECT_EQ(sentinel, n);
		const auto fallback = e.getNormal();
		EXPECT_EQ(Vector3d<T>(0, 0, 0), fallback);
	}
}

// Tiny but valid axes are scale-independent: the normal is still found.
TYPED_TEST(Ellipse3dTypedTest, TryGetNormalIsScaleInvariant)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Ellipse3d<T> e(Vector3d<T>(0, 0, 0), Vector3d<T>(1.0e-3, 0, 0), Vector3d<T>(0, 1.0e-3, 0));
	Vector3d<T> n;
	ASSERT_TRUE(e.tryGetNormal(n));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 1), n, tol));
}
