#include "gtest/gtest.h"

#include "../Math/Ellipsoid3d.h"

#include "PrimitiveTestSupport.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-12f;
}

TEST(Ellipsoid3dTest, TestGetPosition)
{
	Ellipsoid3df e(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));
	const auto actual = e.getPosition(0, 0);
	const Vector3df expected(0, 0, 2);
	EXPECT_TRUE(::areSame(expected, actual, tolerance));
}

template<typename T>
class Ellipsoid3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Ellipsoid3dTypedTest, PrimitiveScalars);

// Invariant: with axis-aligned semi-axes (a, b, c) every sample satisfies
// (x/a)^2 + (y/b)^2 + (z/c)^2 = 1 (relative to the center).
TYPED_TEST(Ellipsoid3dTypedTest, SamplesSatisfyEllipsoidEquation)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Vector3d<T> center(1, -2, 3);
	const Ellipsoid3d<T> e(center, Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0), Vector3d<T>(0, 0, 4));
	for (int i = 0; i <= 8; ++i) {
		for (int j = 0; j < 8; ++j) {
			const auto p = e.getPosition(T(i) / T(8), T(j) / T(8)) - center;
			const T lhs = (p.x / 2) * (p.x / 2) + (p.y / 3) * (p.y / 3) + (p.z / 4) * (p.z / 4);
			EXPECT_NEAR(T(1), lhs, tol);
		}
	}
}

// With unit axes the ellipsoid is the unit sphere: distance from center is 1.
TYPED_TEST(Ellipsoid3dTypedTest, DefaultIsUnitSphere)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Ellipsoid3d<T> e;
	for (int i = 0; i <= 4; ++i) {
		EXPECT_NEAR(T(1), getLength(e.getPosition(T(i) / T(4), T(0.3))), tol);
	}
}

TYPED_TEST(Ellipsoid3dTypedTest, PolesLieOnWVec)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Ellipsoid3d<T> e(Vector3d<T>(1, 1, 1), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0), Vector3d<T>(0, 0, 4));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 1, 5), e.getPosition(T(0), T(0.7)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 1, -3), e.getPosition(T(1), T(0.2)), tol));
}

// Degenerate: all-zero axes collapse to the center (no NaN).
TYPED_TEST(Ellipsoid3dTypedTest, ZeroAxesCollapseToCenter)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>();
	const Vector3d<T> zero(0, 0, 0);
	const Ellipsoid3d<T> e(Vector3d<T>(1, 2, 3), zero, zero, zero);
	EXPECT_TRUE(::areSame(Vector3d<T>(1, 2, 3), e.getPosition(T(0.3), T(0.6)), tol));
}

// Negating every axis maps each sample through the center (point reflection).
TYPED_TEST(Ellipsoid3dTypedTest, NegatedAxesReflectThroughCenter)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Vector3d<T> c(1, 2, 3);
	const Ellipsoid3d<T> a(c, Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0), Vector3d<T>(0, 0, 4));
	const Ellipsoid3d<T> b(c, Vector3d<T>(-2, 0, 0), Vector3d<T>(0, -3, 0), Vector3d<T>(0, 0, -4));
	const auto pa = a.getPosition(T(0.3), T(0.6)) - c;
	const auto pb = b.getPosition(T(0.3), T(0.6)) - c;
	EXPECT_TRUE(::areSame(pa, -pb, tol));
}
