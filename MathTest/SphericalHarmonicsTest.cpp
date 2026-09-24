#include "gtest/gtest.h"

#include "../Math/SphericalHarmonics.h"

#include <cmath>

using namespace Phantom::Math;

namespace {
constexpr float pi = 3.14159265358979323846f;
}

TEST(SphericalHarmonicsTest, BasisIsOrthonormalForConstantAndLinearTerms)
{
    constexpr int samples = 128;
    std::vector<glm::vec3> directions;
    std::vector<glm::vec3> values;
    directions.reserve(samples * samples);
    values.reserve(samples * samples);
    for (int y = 0; y < samples; ++y) {
        const float z = 1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / samples;
        const float radius = std::sqrt(std::max(0.0f, 1.0f - z * z));
        for (int x = 0; x < samples; ++x) {
            const float phi = 2.0f * pi * (static_cast<float>(x) + 0.5f) / samples;
            directions.emplace_back(radius * std::cos(phi), radius * std::sin(phi), z);
            values.emplace_back(1.0f);
        }
    }

    const auto coefficients = projectSamples(directions, values, 1);
    EXPECT_NEAR(std::sqrt(4.0f * pi), coefficients.coefficients[0].x, 2.0e-3f);
    EXPECT_NEAR(0.0f, coefficients.coefficients[1].x, 2.0e-3f);
    EXPECT_NEAR(0.0f, coefficients.coefficients[2].x, 2.0e-3f);
    EXPECT_NEAR(0.0f, coefficients.coefficients[3].x, 2.0e-3f);
}

TEST(SphericalHarmonicsTest, HGConvolutionUsesGToTheBandPower)
{
    SHRGB value;
    value.degree = 2;
    value.coefficients[0] = glm::vec3(1.0f);
    value.coefficients[1] = glm::vec3(2.0f);
    value.coefficients[4] = glm::vec3(3.0f);
    value.coefficients[6] = glm::vec3(4.0f);

    const auto convolved = convolveHenyeyGreenstein(value, 0.5f);
    EXPECT_FLOAT_EQ(1.0f, convolved.coefficients[0].x);
    EXPECT_FLOAT_EQ(1.0f, convolved.coefficients[1].x);
    EXPECT_FLOAT_EQ(0.75f, convolved.coefficients[4].x);
    EXPECT_FLOAT_EQ(1.0f, convolved.coefficients[6].x);
}

TEST(SphericalHarmonicsTest, OctahedralConstantMapHasNoDirectionalEnergy)
{
    const std::vector<glm::vec3> map(32 * 16, glm::vec3(2.0f));
    const auto coefficients = projectOctahedralMap(map, 32, 16, 2);
    EXPECT_NEAR(2.0f * std::sqrt(4.0f * pi), coefficients.coefficients[0].x, 0.04f);
    for (int i = 1; i < 9; ++i)
        EXPECT_NEAR(0.0f, glm::length(coefficients.coefficients[static_cast<std::size_t>(i)]), 0.04f);
}

TEST(SphericalHarmonicsTest, LanczosWindowPreservesDC)
{
    SHRGB value;
    value.degree = 2;
    value.coefficients.fill(glm::vec3(1.0f));
    applyLanczosSigmaWindow(value);
    EXPECT_FLOAT_EQ(1.0f, value.coefficients[0].x);
    EXPECT_LT(value.coefficients[1].x, 1.0f);
    EXPECT_LT(value.coefficients[4].x, value.coefficients[1].x);
}

TEST(SphericalHarmonicsTest, OctahedralTexelSolidAnglesMatchTheSphere)
{
    const auto solidAngles = octahedralTexelSolidAngles(16, 16);
    ASSERT_EQ(256U, solidAngles.size());
    float sum = 0.0f;
    for (const float solidAngle : solidAngles)
        sum += solidAngle;
    EXPECT_NEAR(4.0f * pi, sum, 1.0e-4f);
    // Octant face-centre texels cover more solid angle than the +z axis texel.
    EXPECT_GT(solidAngles[10 * 16 + 10], 2.5f * solidAngles[8 * 16 + 8]);
}

TEST(SphericalHarmonicsTest, OctahedralProjectionOfLinearFunctionIsUnbiased)
{
    // f(d) = d.z projects onto Y_1^0 with coefficient 4*pi/3 * Y1 and has no
    // DC term. Uniform texel weights bias both because texels are not equal-area.
    constexpr int size = 32;
    std::vector<glm::vec3> map;
    map.reserve(size * size);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            map.emplace_back(octahedralDirection(x, y, size, size).z);

    const auto coefficients = projectOctahedralMap(map, size, size, 1);
    EXPECT_NEAR(0.0f, coefficients.coefficients[0].x, 5.0e-3f);
    EXPECT_NEAR(4.0f * pi / 3.0f * 0.4886025119029199f, coefficients.coefficients[3].x, 5.0e-3f);
}

TEST(SphericalHarmonicsTest, OctahedralTexelRoundTrips)
{
    constexpr int size = 16;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const glm::ivec2 texel = octahedralTexel(octahedralDirection(x, y, size, size), size, size);
            EXPECT_EQ(x, texel.x);
            EXPECT_EQ(y, texel.y);
        }
    }
}
