#include "gtest/gtest.h"

#include "../Core/Box3d.h"
#include "../Core/Matrix4d.h"
#include "../Core/Quaternion.h"
#include "../Core/Sphere3d.h"
#include "../Core/Vector3d.h"
#include "PrimitiveTestSupport.h"

#include "../../ThirdParty/glm/glm/ext/matrix_transform.hpp"
#include "../../ThirdParty/glm/glm/gtc/quaternion.hpp"

using namespace Phantom::Math;

template<typename T>
class MathInvariantTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(MathInvariantTypedTest, PrimitiveScalars);

// areSame is symmetric. It uses absolute distance, so the tolerance is in world units.
TYPED_TEST(MathInvariantTypedTest, AreSameIsSymmetric)
{
	using T = TypeParam;
	const Vector3d<T> a(1, 2, 3);
	const Vector3d<T> b(1.5, 2, 3);
	EXPECT_EQ(::areSame(a, b, T(0.6)), ::areSame(b, a, T(0.6)));
	EXPECT_EQ(::areSame(a, b, T(0.4)), ::areSame(b, a, T(0.4)));
	EXPECT_TRUE(::areSame(a, b, T(0.6)));
	EXPECT_FALSE(::areSame(a, b, T(0.4)));
	// Strict "<": a distance equal to the tolerance is not "same".
	EXPECT_FALSE(::areSame(a, Vector3d<T>(2, 2, 3), T(1)));
}

// M * M^-1 = I. Entries are O(1) so an absolute tolerance is appropriate; the
// matrix is well conditioned (rotation + translation + scale 2).
TYPED_TEST(MathInvariantTypedTest, Matrix4InverseRoundTrip)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	Matrix4d<T> m(1);
	m = glm::translate(m, Vector3d<T>(1, -2, 3));
	m = glm::rotate(m, T(0.7), glm::normalize(Vector3d<T>(1, 2, 3)));
	m = glm::scale(m, Vector3d<T>(2, 2, 2));
	const Matrix4d<T> id = m * glm::inverse(m);
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			EXPECT_NEAR(c == r ? T(1) : T(0), id[c][r], tol);
		}
	}
}

// Rotation preserves length, and q^-1 undoes q.
TYPED_TEST(MathInvariantTypedTest, QuaternionRotationPreservesLengthAndInverts)
{
	using T = TypeParam;
	const T tol = primitiveTolerance<T>() * 10;
	const auto q = glm::angleAxis(T(1.1), glm::normalize(Vector3d<T>(1, 1, 0)));
	const Vector3d<T> v(3, -4, 12);
	const auto rotated = q * v;
	EXPECT_NEAR(getLength(v), getLength(rotated), tol * 13);
	EXPECT_TRUE(::areSame(v, glm::inverse(q) * rotated, tol * 13));
}

// Box3d::contains is strict (boundary excluded) and tolerance grows the box.
TYPED_TEST(MathInvariantTypedTest, Box3dContainsExcludesSurfaceAndHonorsTolerance)
{
	using T = TypeParam;
	const Box3d<T> box(Vector3d<T>(0, 0, 0), Vector3d<T>(2, 2, 2));
	EXPECT_TRUE(box.contains(Vector3d<T>(1, 1, 1), T(0)));
	EXPECT_FALSE(box.contains(Vector3d<T>(2, 1, 1), T(0)));   // on a face
	EXPECT_FALSE(box.contains(Vector3d<T>(0, 0, 0), T(0)));   // on a corner
	EXPECT_TRUE(box.contains(Vector3d<T>(2, 1, 1), T(0.5)));  // inside the grown box
	EXPECT_FALSE(box.contains(Vector3d<T>(2.5, 1, 1), T(0.5))); // on the grown face
	EXPECT_FALSE(box.contains(Vector3d<T>(3, 1, 1), T(0.5)));
}

// A zero-volume box contains nothing at tolerance 0; positive tolerance gives it
// a finite neighbourhood.
TYPED_TEST(MathInvariantTypedTest, DegenerateBox3dContainsOnlyWithTolerance)
{
	using T = TypeParam;
	const Box3d<T> point(Vector3d<T>(1, 1, 1));
	EXPECT_FALSE(point.contains(Vector3d<T>(1, 1, 1), T(0)));
	EXPECT_TRUE(point.contains(Vector3d<T>(1, 1, 1), T(0.1)));
	EXPECT_TRUE(point.contains(Vector3d<T>(1.05, 1, 1), T(0.1)));
	EXPECT_FALSE(point.contains(Vector3d<T>(1.2, 1, 1), T(0.1)));
}

// Containment is monotone: a box contained by another stays contained as the
// tolerance grows, and a point in a contained box is contained by the outer box.
TYPED_TEST(MathInvariantTypedTest, Box3dContainmentIsMonotone)
{
	using T = TypeParam;
	const Box3d<T> outer(Vector3d<T>(0, 0, 0), Vector3d<T>(10, 10, 10));
	const Box3d<T> inner(Vector3d<T>(2, 2, 2), Vector3d<T>(4, 4, 4));
	EXPECT_TRUE(outer.contains(inner, T(0)));
	EXPECT_TRUE(outer.contains(inner, T(1)));
	EXPECT_FALSE(inner.contains(outer, T(0)));
	EXPECT_TRUE(outer.contains(inner.getCenter(), T(0)));
}

// Sphere3d::contains is strict like Box3d, and the bounding box contains every
// surface sample (up to a small tolerance for surface points on the box faces).
TYPED_TEST(MathInvariantTypedTest, SphereContainmentAndBoundingBox)
{
	using T = TypeParam;
	const Sphere3d<T> s(Vector3d<T>(1, 2, 3), T(2));
	EXPECT_TRUE(s.contains(Vector3d<T>(1, 2, 3), T(0)));
	EXPECT_FALSE(s.contains(Vector3d<T>(3, 2, 3), T(0)));  // exactly on the surface
	EXPECT_TRUE(s.contains(Vector3d<T>(3, 2, 3), T(0.1)));
	const auto box = s.getBoundingBox();
	for (int i = 0; i <= 6; ++i) {
		for (int j = 0; j < 6; ++j) {
			const auto p = s.getPosition(T(i) / T(6), T(j) / T(6));
			EXPECT_TRUE(box.contains(p, primitiveTolerance<T>() * 10));
		}
	}
}
