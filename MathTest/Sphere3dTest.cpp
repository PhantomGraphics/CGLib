#include "gtest/gtest.h"

#include "../Math/Sphere3d.h"

#include "PrimitiveTestSupport.h"

#include <limits>

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-12f;
}

TEST(Sphere3dTest, TestGetPosition)
{
	const Sphere3df s(Vector3df(0.0, 0.0, 0.0), 1.0);

	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getPosition(0.0, 0.0, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getPosition(0.0, 0.5, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getPosition(0.0, 1.0, 1.0), tolerance));

	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getPosition(0.0, 0.5, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df(-1, 0, 0), s.getPosition(0.5, 0.5, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getPosition(1.0, 0.5, 1.0), tolerance));

	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getPosition(1.0, 0.0, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getPosition(1.0, 0.5, 1.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getPosition(1.0, 1.0, 1.0), tolerance));
}

TEST(Sphere3dTest, TestGetNormal)
{
	const Sphere3df s(Vector3df(0.0, 0.0, 0.0), 1.0);

	EXPECT_TRUE(areSame(Vector3df( 0, 0, 1), s.getNormal(0.0, 0.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 1, 0, 0), s.getNormal(0.5, 0.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df( 0, 0,-1), s.getNormal(1.0, 0.0), tolerance));
}

TEST(Sphere3dTest, TestContains)
{
	const Sphere3df s(Vector3df(0.0f, 0.0f, 0.0f), 1.0f);

	EXPECT_TRUE(s.contains(Vector3df(0.5f, 0.0f, 0.0f), tolerance));
	EXPECT_FALSE(s.contains(Vector3df(2.0f, 0.0f, 0.0f), tolerance));
	EXPECT_TRUE(s.contains(Vector3df(1.0f, 0.0f, 0.0f), 0.01f));
}

template<typename T>
class Sphere3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Sphere3dTypedTest, PrimitiveScalars);

// The normal is radial: normalize(position - center), independent of radius.
TYPED_TEST(Sphere3dTypedTest, NormalIsRadialAndUnit)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Sphere3d<T> s(Vector3d<T>(1, 2, 3), T(4));
	for (int i = 0; i <= 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			const T u = T(i) / T(4), v = T(j) / T(4);
			const auto n = s.getNormal(u, v);
			EXPECT_NEAR(T(1), getLength(n), tol);
			EXPECT_TRUE(::areSame(s.getPosition(u, v), s.getCenter() + T(4) * n, tol * 10));
		}
	}
}

TYPED_TEST(Sphere3dTypedTest, ZeroRadiusNormalIsZeroAndTryFails)
{
	using T = TypeParam;
	const Sphere3d<T> s(Vector3d<T>(1, 2, 3), T(0));
	EXPECT_EQ(Vector3d<T>(0, 0, 0), s.getNormal(T(0.3), T(0.6)));
	const Vector3d<T> sentinel(7, 8, 9);
	Vector3d<T> n = sentinel;
	EXPECT_FALSE(s.tryGetNormal(T(0.3), T(0.6), n));
	EXPECT_EQ(sentinel, n);
}

TYPED_TEST(Sphere3dTypedTest, NonFiniteRadiusTryFails)
{
	using T = TypeParam;
	const Sphere3d<T> s(Vector3d<T>(0, 0, 0), std::numeric_limits<T>::infinity());
	Vector3d<T> n(7, 8, 9);
	EXPECT_FALSE(s.tryGetNormal(T(0.3), T(0.6), n));
	EXPECT_EQ(Vector3d<T>(7, 8, 9), n);
}

TYPED_TEST(Sphere3dTypedTest, TryGetNormalMatchesGetNormal)
{
	using T = TypeParam;
	const Sphere3d<T> s(Vector3d<T>(0, 0, 0), T(2));
	Vector3d<T> n;
	ASSERT_TRUE(s.tryGetNormal(T(0.5), T(0), n));
	EXPECT_EQ(s.getNormal(T(0.5), T(0)), n);
}
