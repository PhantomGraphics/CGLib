#include "gtest/gtest.h"
#include "../Math/Matrix4d.h"
#include "../Math/pi.h"
#include "../ThirdParty/glm-0.9.9.8/glm/ext/matrix_transform.hpp"

using namespace Phantom::Math;

namespace {
	const double tolerance = 1.0e-12;
	const double pi = static_cast<double>(PI);
}

TEST(Matrix4dTest, DefaultConstructor)
{
    Matrix4dd m{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_EQ(m[i][j], 0.0);
        }
    }
}

TEST(Matrix4dTest, Identity)
{
    auto m = glm::identity<Matrix4dd>();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_EQ(m[i][j], i == j ? 1.0 : 0.0);
        }
    }
}

TEST(Matrix4dTest, Multiplication)
{
    auto m1 = glm::identity<Matrix4dd>();
    auto m2 = glm::identity<Matrix4dd>();
    auto m3 = m1 * m2;
    EXPECT_EQ(m3, glm::identity<Matrix4dd>());
}

TEST(Matrix4dTest, Transpose) {
    Matrix4dd m{ {1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16} };
    auto t = glm::transpose(m);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_EQ(m[i][j], t[j][i]);
}

TEST(Matrix4dTest, Inverse) {
    Matrix4dd m = glm::identity<Matrix4dd>();
    auto inv = glm::inverse(m);
    EXPECT_EQ(inv, glm::identity<Matrix4dd>());
}

TEST(Matrix4dTest, Translation) {
    auto m = glm::translate(glm::identity<Matrix4dd>(), glm::dvec3(1.0, 2.0, 3.0));
    EXPECT_EQ(m[3][0], 1.0);
    EXPECT_EQ(m[3][1], 2.0);
    EXPECT_EQ(m[3][2], 3.0);
}

TEST(Matrix4dTest, Scale)
{
	auto m = glm::scale(glm::identity<Matrix4dd>(), glm::dvec3(2.0, 3.0, 4.0));
	const glm::dvec4 v(1.0, 1.0, 1.0, 1.0);
	const auto actual = m * v;

	EXPECT_NEAR(2.0, actual.x, tolerance);
	EXPECT_NEAR(3.0, actual.y, tolerance);
	EXPECT_NEAR(4.0, actual.z, tolerance);
	EXPECT_NEAR(1.0, actual.w, tolerance);
}

TEST(Matrix4dTest, TranslationThenRotation)
{
	const auto translation = glm::translate(glm::identity<Matrix4dd>(), glm::dvec3(1.0, 2.0, 3.0));
	const auto rotation = glm::rotate(glm::identity<Matrix4dd>(), 0.5 * pi, glm::dvec3(0.0, 0.0, 1.0));
	const auto composed = translation * rotation;
	const glm::dvec4 point(1.0, 0.0, 0.0, 1.0);
	const auto actual = composed * point;

	EXPECT_NEAR(1.0, actual.x, tolerance);
	EXPECT_NEAR(3.0, actual.y, tolerance);
	EXPECT_NEAR(3.0, actual.z, tolerance);
	EXPECT_NEAR(1.0, actual.w, tolerance);
}

TEST(Matrix4dTest, InverseOfTranslation)
{
	const auto translation = glm::translate(glm::identity<Matrix4dd>(), glm::dvec3(1.0, 2.0, 3.0));
	const auto inverse = glm::inverse(translation);
	const auto actual = translation * inverse;

	EXPECT_EQ(actual, glm::identity<Matrix4dd>());
}