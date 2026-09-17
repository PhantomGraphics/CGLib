#include "gtest/gtest.h"

#include "../SceneRuntime/Transform.h"

#include <glm/gtc/quaternion.hpp>

using namespace Phantom::SceneRuntime;

TEST(Transform, IdentityIsIdentityMatrix)
{
    Transform t;
    const Phantom::Math::Matrix4df m = t.toMatrix();
    EXPECT_EQ(m, Phantom::Math::Matrix4df(1.0f));
}

TEST(Transform, TranslationOnlyMovesOrigin)
{
    Transform t;
    t.translation = { 1.0f, 2.0f, 3.0f };
    const Phantom::Math::Matrix4df m = t.toMatrix();
    const glm::vec4 origin = m * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(origin.x, 1.0f);
    EXPECT_FLOAT_EQ(origin.y, 2.0f);
    EXPECT_FLOAT_EQ(origin.z, 3.0f);
}

TEST(Transform, ScaleOnlyScalesAxisVector)
{
    Transform t;
    t.scale = { 2.0f, 3.0f, 4.0f };
    const Phantom::Math::Matrix4df m = t.toMatrix();
    const glm::vec4 p = m * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_FLOAT_EQ(p.x, 2.0f);
    EXPECT_FLOAT_EQ(p.y, 3.0f);
    EXPECT_FLOAT_EQ(p.z, 4.0f);
}

TEST(Transform, RotationOnlyRotates90DegreesAboutZ)
{
    Transform t;
    t.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    const Phantom::Math::Matrix4df m = t.toMatrix();
    const glm::vec4 p = m * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(p.x, 0.0f, 1.0e-5f);
    EXPECT_NEAR(p.y, 1.0f, 1.0e-5f);
    EXPECT_NEAR(p.z, 0.0f, 1.0e-5f);
}

TEST(Transform, EqualityComparesAllThreeFields)
{
    Transform a;
    Transform b;
    EXPECT_EQ(a, b);

    Transform c = a;
    c.translation = { 1.0f, 0.0f, 0.0f };
    EXPECT_NE(a, c);

    Transform d = a;
    d.rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    EXPECT_NE(a, d);

    Transform e = a;
    e.scale = { 2.0f, 1.0f, 1.0f };
    EXPECT_NE(a, e);
}
