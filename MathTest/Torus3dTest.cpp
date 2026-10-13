#include "gtest/gtest.h"

#include "../Math/Torus3d.h"

#include "PrimitiveTestSupport.h"

#include <limits>

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-5f;
}

TEST(Torus3dTest, TestGetPosition)
{
	const Torus3df t(Vector3df(0, 0, 0), Vector3df(0, 1, 0), 1.0f, 0.25f);

	// u = 0, v = 0 -> outer equator point of the tube closest to +Z
	EXPECT_TRUE(::areSame(Vector3df(0, 0, 1.25f), t.getPosition(0.0f, 0.0f), tolerance));

	// u = 0, v = 0.25 -> top of the tube (+Y) above the +Z ring point
	EXPECT_TRUE(::areSame(Vector3df(0, 0.25f, 1.0f), t.getPosition(0.0f, 0.25f), tolerance));

	// u = 0.25, v = 0 -> outer equator point of the tube closest to +X
	EXPECT_TRUE(::areSame(Vector3df(1.25f, 0, 0), t.getPosition(0.25f, 0.0f), tolerance));
}

TEST(Torus3dTest, TestAccessors)
{
	const Torus3df t(Vector3df(1, 2, 3), Vector3df(0, 1, 0), 2.0f, 0.5f);
	EXPECT_TRUE(::areSame(Vector3df(1, 2, 3), t.getCenter(), tolerance));
	EXPECT_FLOAT_EQ(2.0f, t.getMajorRadius());
	EXPECT_FLOAT_EQ(0.5f, t.getMinorRadius());
}

template<typename T>
class Torus3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Torus3dTypedTest, PrimitiveScalars);

TYPED_TEST(Torus3dTypedTest, NormalIsNormalizedAtConstruction)
{
	using T = TypeParam;
	const Torus3d<T> t(Vector3d<T>(0, 0, 0), Vector3d<T>(0, 0, 5), T(2), T(0.5));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 1), t.getNormal(), primitiveTolerance<T>()));
}

// A zero-length or non-finite axis falls back to +Y rather than storing NaN.
TYPED_TEST(Torus3dTypedTest, InvalidAxisFallsBackToDefaultAxis)
{
	using T = TypeParam;
	const T nan = std::numeric_limits<T>::quiet_NaN();
	for (const Vector3d<T> bad : { Vector3d<T>(0, 0, 0), Vector3d<T>(nan, 0, 0) }) {
		const Torus3d<T> t(Vector3d<T>(0, 0, 0), bad, T(2), T(0.5));
		EXPECT_EQ(Vector3d<T>(0, 1, 0), t.getNormal());
		const auto p = t.getPosition(T(0.1), T(0.2));
		EXPECT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
	}
}

// Every sample lies at the minor radius from the ring of major radius around the axis.
TYPED_TEST(Torus3dTypedTest, SamplesLieAtMinorRadiusFromRing)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const Vector3d<T> axis = glm::normalize(Vector3d<T>(1, 2, 3));
	const Torus3d<T> t(Vector3d<T>(1, 1, 1), axis, T(2), T(0.5));
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < 8; ++j) {
			const auto p = t.getPosition(T(i) / T(8), T(j) / T(8)) - Vector3d<T>(1, 1, 1);
			const T height = glm::dot(p, axis);
			const T radial = getLength(p - height * axis);
			EXPECT_NEAR(T(0.5), std::sqrt((radial - 2) * (radial - 2) + height * height), tol);
		}
	}
}
