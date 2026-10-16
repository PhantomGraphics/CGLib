#include "gtest/gtest.h"

#include "../Core/Cone3d.h"

#include "PrimitiveTestSupport.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-5f;
}

TEST(Cone3dTest, TestGetPosition)
{
	Cone3df c(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));

	// v = 0 -> base circle (full radius)
	EXPECT_TRUE(::areSame(Vector3df(1, 0, 0), c.getPosition(0.0f, 0.0f), tolerance));

	// v = 1 -> apex, independent of u
	EXPECT_TRUE(::areSame(c.getApex(), c.getPosition(0.0f, 1.0f), tolerance));
	EXPECT_TRUE(::areSame(c.getApex(), c.getPosition(0.25f, 1.0f), tolerance));
}

TEST(Cone3dTest, TestGetApex)
{
	const Cone3df c(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 2), c.getApex(), tolerance));
}


template<typename T>
class Cone3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Cone3dTypedTest, PrimitiveScalars);

// Invariant: at height v the ring has radius (1 - v) * |uvec| and lies at v * wvec.
TYPED_TEST(Cone3dTypedTest, RingRadiusShrinksLinearly)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Vector3d<T> base(1, 2, 3);
	const Cone3d<T> c(base, Vector3d<T>(2, 0, 0), Vector3d<T>(0, 2, 0), Vector3d<T>(0, 0, 4));
	for (const T v : { T(0), T(0.25), T(0.5), T(0.9) }) {
		for (int i = 0; i < 8; ++i) {
			const auto p = c.getPosition(T(i) / T(8), v) - base;
			EXPECT_NEAR(v * 4, p.z, tol);
			EXPECT_NEAR((T(1) - v) * 2, std::sqrt(p.x * p.x + p.y * p.y), tol);
		}
	}
}

TYPED_TEST(Cone3dTypedTest, ApexIsBottomPlusWVecAndReachedAtVOne)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cone3d<T> c(Vector3d<T>(1, 1, 1), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0), Vector3d<T>(0, 0, 2));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 1, 3), c.getApex(), tol));
	EXPECT_TRUE(::areSame(c.getApex(), c.getPosition(T(0.77), T(1)), tol));
}

// Degenerate: zero-radius base collapses the cone to its axis segment (no NaN).
TYPED_TEST(Cone3dTypedTest, ZeroRadiusCollapsesToAxis)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Vector3d<T> zero(0, 0, 0);
	const Cone3d<T> c(zero, zero, zero, Vector3d<T>(0, 0, 2));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 1), c.getPosition(T(0.4), T(0.5)), tol));
}

// Degenerate: zero height flattens the cone to the base disc: v = 1 reaches the center.
TYPED_TEST(Cone3dTypedTest, ZeroHeightFlattensToDisc)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Cone3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0), Vector3d<T>(0, 0, 0));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 0), c.getPosition(T(0.4), T(1)), tol));
	EXPECT_NEAR(T(0.5), getLength(c.getPosition(T(0.4), T(0.5))), tol);
}

TYPED_TEST(Cone3dTypedTest, UIsPeriodic)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Cone3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0), Vector3d<T>(0, 0, 1));
	EXPECT_TRUE(::areSame(c.getPosition(T(0.2), T(0.3)), c.getPosition(T(1.2), T(0.3)), tol));
}
