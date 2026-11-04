#include "gtest/gtest.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

#include "../Core/SkinningCpu.h"

using namespace Phantom::Animation;

namespace {
SkinVertex vertex(glm::vec3 p, glm::vec3 n, glm::ivec4 bones, glm::vec4 weights)
{
    SkinVertex v;
    v.position = p; v.normal = n; v.boneIndices = bones; v.boneWeights = weights;
    return v;
}
}

TEST(SkinningCpu, SingleBoneTranslatesPositionAndKeepsNormal)
{
    std::vector<glm::mat4> m{ glm::translate(glm::mat4(1.f), glm::vec3(1, 2, 3)) };
    std::vector<SkinVertex> v{ vertex({ 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0, 0 }, { 1, 0, 0, 0 }) };
    float p[3], n[3];
    skinVertices(v.data(), 1, m.data(), m.size(), p, n);
    EXPECT_NEAR(p[0], 2.f, 1e-6f); EXPECT_NEAR(p[1], 2.f, 1e-6f); EXPECT_NEAR(p[2], 3.f, 1e-6f);
    EXPECT_NEAR(n[1], 1.f, 1e-6f);
}

TEST(SkinningCpu, RotationRotatesNormalAndBlendsHalfway)
{
    const glm::mat4 rot = glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(0, 0, 1));
    std::vector<glm::mat4> m{ glm::mat4(1.f), rot };
    std::vector<SkinVertex> v{
        vertex({ 1, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }),
        vertex({ 1, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0, 0 }, { 0.5f, 0.5f, 0, 0 }),
    };
    float p[6], n[6];
    skinVertices(v.data(), 2, m.data(), m.size(), p, n);
    EXPECT_NEAR(p[0], 0.f, 1e-6f); EXPECT_NEAR(p[1], 1.f, 1e-6f);   // +90 degrees about Z: (1,0,0) -> (0,1,0)
    EXPECT_NEAR(n[0], 0.f, 1e-6f); EXPECT_NEAR(n[1], 1.f, 1e-6f);
    EXPECT_NEAR(p[3], 0.5f, 1e-6f); EXPECT_NEAR(p[4], 0.5f, 1e-6f); // halfway between (1,0,0) and (0,1,0)
    EXPECT_NEAR(glm::length(glm::vec3(n[3], n[4], n[5])), 1.f, 1e-6f);
}

TEST(SkinningCpu, InvalidInfluencesAreSkippedAndNullNormalsAllowed)
{
    std::vector<glm::mat4> m{ glm::translate(glm::mat4(1.f), glm::vec3(5, 0, 0)) };
    std::vector<SkinVertex> v{
        vertex({ 1, 0, 0 }, { 0, 0, 1 }, { 7, -1, 0, 0 }, { 0.5f, 0.5f, 0, 0 }),   // both influences invalid
        vertex({ 1, 0, 0 }, { 0, 0, 1 }, { 0, 9, 0, 0 }, { 0.5f, 0.5f, 0, 0 }),    // one valid at half weight
    };
    float p[6];
    skinVertices(v.data(), 2, m.data(), m.size(), p, nullptr);
    EXPECT_EQ(p[0], 0.f); EXPECT_EQ(p[1], 0.f); EXPECT_EQ(p[2], 0.f);
    EXPECT_NEAR(p[3], 3.f, 1e-6f);   // 0.5 * (1 + 5)
    float n[3];
    skinVertices(v.data(), 1, m.data(), m.size(), p, n);
    EXPECT_NEAR(n[2], 1.f, 1e-6f);   // no valid influence: the bind-pose normal is kept
}
