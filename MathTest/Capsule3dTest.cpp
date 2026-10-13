#include "gtest/gtest.h"

#include "../Math/Capsule3d.h"

#include "PrimitiveTestSupport.h"

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-5f;
}

TEST(Capsule3dTest, TestGetPosition)
{
	// radius = 1 (|uvec|), axis along +Z with cylindrical length 2.
	Capsule3df c(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));

	// v = 0 -> bottom pole (tip of the bottom hemisphere), independent of u.
	EXPECT_TRUE(::areSame(Vector3df(0, 0, -1), c.getPosition(0.0f, 0.0f), tolerance));

	// v = 1 -> top pole (tip of the top hemisphere), independent of u.
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 3), c.getPosition(0.0f, 1.0f), tolerance));

	// v = 1/3 -> equator ring where the bottom cap meets the cylindrical side.
	EXPECT_TRUE(::areSame(Vector3df(1, 0, 0), c.getPosition(0.0f, 1.0f / 3.0f), tolerance));

	// v = 2/3 -> equator ring where the cylindrical side meets the top cap.
	EXPECT_TRUE(::areSame(Vector3df(1, 0, 2), c.getPosition(0.0f, 2.0f / 3.0f), tolerance));

	// v = 0.5 -> midpoint of the cylindrical side.
	EXPECT_TRUE(::areSame(Vector3df(1, 0, 1), c.getPosition(0.0f, 0.5f), tolerance));
}

TEST(Capsule3dTest, TestAccessors)
{
	const Capsule3df c(Vector3df(0, 0, 0), Vector3df(1, 0, 0), Vector3df(0, 1, 0), Vector3df(0, 0, 2));
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 0), c.getBottom(), tolerance));
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 2), c.getTop(), tolerance));
	EXPECT_FLOAT_EQ(1.0f, c.getRadius());
}


template<typename T>
class Capsule3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Capsule3dTypedTest, PrimitiveScalars);

namespace {
	// Distance from p to the segment [a, b].
	template<typename T>
	T distanceToSegment(const Vector3d<T>& p, const Vector3d<T>& a, const Vector3d<T>& b)
	{
		const auto ab = b - a;
		const T t = glm::clamp(glm::dot(p - a, ab) / glm::dot(ab, ab), T(0), T(1));
		return getLength(p - (a + t * ab));
	}
}

// Invariant: every surface sample is exactly `radius` away from the core segment.
TYPED_TEST(Capsule3dTypedTest, SamplesLieAtConstantDistanceFromCoreSegment)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Vector3d<T> bottom(1, 2, 3);
	const Capsule3d<T> c(bottom, Vector3d<T>(1.5, 0, 0), Vector3d<T>(0, 1.5, 0), Vector3d<T>(0, 0, 4));
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j <= 12; ++j) {
			const auto p = c.getPosition(T(i) / T(8), T(j) / T(12));
			EXPECT_NEAR(c.getRadius(), distanceToSegment(p, c.getBottom(), c.getTop()), tol);
		}
	}
}

// Invariant: the surface is continuous across the band boundaries v = 1/3, 2/3.
TYPED_TEST(Capsule3dTypedTest, ContinuousAcrossBandBoundaries)
{
	using T = TypeParam;
	const Capsule3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0), Vector3d<T>(0, 0, 2));
	const T eps = T(1.0e-4);
	for (const T v : { T(1) / T(3), T(2) / T(3) }) {
		const auto below = c.getPosition(T(0.2), v - eps);
		const auto above = c.getPosition(T(0.2), v + eps);
		// Surface speed is ~5.4 per unit v here, so a 2*eps step moves ~1.1e-3;
		// a jump at the band boundary would be orders larger.
		EXPECT_LT(getLength(above - below), T(2.0e-3));
	}
}

// Poles are independent of u.
TYPED_TEST(Capsule3dTypedTest, PolesAreIndependentOfU)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Capsule3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0), Vector3d<T>(0, 0, 2));
	for (const T u : { T(0), T(0.3), T(0.7) }) {
		EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, -1), c.getPosition(u, T(0)), tol));
		EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 3), c.getPosition(u, T(1)), tol));
	}
}

// Axis direction need not be +Z: poles extend by the radius along wvec.
TYPED_TEST(Capsule3dTypedTest, TiltedAxisPolesExtendAlongWVec)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Capsule3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(0, 0, 1), Vector3d<T>(0, 1, 0), Vector3d<T>(3, 0, 0));
	EXPECT_TRUE(::areSame(Vector3d<T>(-1, 0, 0), c.getPosition(T(0), T(0)), tol));
	EXPECT_TRUE(::areSame(Vector3d<T>(4, 0, 0), c.getPosition(T(0), T(1)), tol));
}

// Target contract for Phase 2: a zero-length axis (wvec = 0) must not produce
// NaN/inf. Disabled until the contract is implemented (it divides by |wvec|
// today); must FAIL when run with --gtest_also_run_disabled_tests.
TYPED_TEST(Capsule3dTypedTest, DISABLED_ZeroLengthAxisProducesFinitePositions)
{
	using T = TypeParam;
	const Capsule3d<T> c(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0), Vector3d<T>(0, 0, 0));
	for (const T v : { T(0), T(0.2), T(0.5), T(1) }) {
		const auto p = c.getPosition(T(0.1), v);
		EXPECT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
	}
}
