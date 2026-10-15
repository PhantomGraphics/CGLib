#include "gtest/gtest.h"

#include "../Math/Rectangle3d.h"

#include "PrimitiveTestSupport.h"

#include <limits>

using namespace Phantom::Math;

namespace {
	const float tolerance = 1.0e-12f;
}

TEST(Rectangle3dTest, TestGetPosition)
{
	const Rectangle3df quad(Vector3df(0, 0, 0), Vector3df(10, 0, 0), Vector3df(0, 10, 0));
	EXPECT_TRUE(areSame(Vector3df(5, 0, 0), quad.getPosition(0.5, 0.0), tolerance));
	EXPECT_TRUE(areSame(Vector3df(0, 5, 0), quad.getPosition(0.0, 0.5), tolerance));
}

TEST(Quad3dTest, TestGetNormal)
{
	const Rectangle3df quad(Vector3df(0, 0, 0), Vector3df(10, 0, 0), Vector3df(0, 10, 0));
	EXPECT_TRUE(areSame(Vector3df(0, 0, 1), quad.getNormal(), tolerance));
}

template<typename T>
class Rectangle3dTypedTest : public ::testing::Test {};
TYPED_TEST_SUITE(Rectangle3dTypedTest, PrimitiveScalars);

TYPED_TEST(Rectangle3dTypedTest, TryGetNormalMatchesGetNormalForValidRectangle)
{
	using T = TypeParam;
	const Rectangle3d<T> r(Vector3d<T>(0, 0, 0), Vector3d<T>(2, 0, 0), Vector3d<T>(0, 3, 0));
	Vector3d<T> n(9, 9, 9);
	ASSERT_TRUE(r.tryGetNormal(n));
	EXPECT_TRUE(::areSame(Vector3d<T>(0, 0, 1), n, primitiveTolerance<T>()));
	EXPECT_EQ(r.getNormal(), n);
}

TYPED_TEST(Rectangle3dTypedTest, TryGetNormalFailsForDegenerateAxesAndKeepsOut)
{
	using T = TypeParam;
	const Vector3d<T> o(0, 0, 0);
	const Vector3d<T> x(1, 0, 0);
	const Rectangle3d<T> cases[] = {
		Rectangle3d<T>(o, o, x),
		Rectangle3d<T>(o, x, Vector3d<T>(-2, 0, 0)),
		Rectangle3d<T>(o, x, Vector3d<T>(0, std::numeric_limits<T>::infinity(), 0)),
	};
	for (const auto& r : cases) {
		const Vector3d<T> sentinel(7, 8, 9);
		Vector3d<T> n = sentinel;
		EXPECT_FALSE(r.tryGetNormal(n));
		EXPECT_EQ(sentinel, n);
	}
}

TYPED_TEST(Rectangle3dTypedTest, GetNormalOfDegenerateRectangleIsZero)
{
	using T = TypeParam;
	const Rectangle3d<T> r(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(2, 0, 0));
	EXPECT_EQ(Vector3d<T>(0, 0, 0), r.getNormal());
}
