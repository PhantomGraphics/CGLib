#include "gtest/gtest.h"

#include "../Volume/VolumeRenderer/ParticleProbeScattering.h"

using namespace Phantom::Volume;
using Phantom::Math::SHRGB;

TEST(ParticleProbeScatteringTest, UniformSelectionIsDeterministicAndUnique)
{
    const std::vector<glm::vec3> positions(32, glm::vec3(0.0f));
    const auto first = ParticleProbeScattering::selectUniform(positions, 8, 1234U);
    const auto second = ParticleProbeScattering::selectUniform(positions, 8, 1234U);
    EXPECT_EQ(first, second);
    EXPECT_EQ(8U, first.size());
    for (std::size_t i = 1; i < first.size(); ++i)
        EXPECT_LT(first[i - 1], first[i]);
}

TEST(ParticleProbeScatteringTest, AdaptiveSelectionPrioritizesImportance)
{
    const std::vector<glm::vec3> positions = {
        glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(3.0f, 0.0f, 0.0f)};
    const auto selected = ParticleProbeScattering::selectAdaptive(
        positions, {0.1f, 0.2f, 10.0f, 0.3f}, 2, 0.0f);
    ASSERT_EQ(2U, selected.size());
    EXPECT_TRUE(std::find(selected.begin(), selected.end(), 2U) != selected.end());
    EXPECT_TRUE(std::find(selected.begin(), selected.end(), 3U) != selected.end());
}

TEST(ParticleProbeScatteringTest, AdaptiveSelectionFillsBudgetWhenSpacingIsTooLarge)
{
    const std::vector<glm::vec3> positions = {
        glm::vec3(0.0f), glm::vec3(0.1f, 0.0f, 0.0f), glm::vec3(0.2f, 0.0f, 0.0f)};
    const auto selected = ParticleProbeScattering::selectAdaptive(
        positions, {1.0f, 2.0f, 3.0f}, 3, 10.0f);
    ASSERT_EQ(3U, selected.size());
    EXPECT_EQ(0U, selected[0]);
    EXPECT_EQ(1U, selected[1]);
    EXPECT_EQ(2U, selected[2]);
}

TEST(ParticleProbeScatteringTest, ImportanceTracksTemporalChange)
{
    SHRGB calm;
    calm.degree = 0;
    calm.coefficients[0] = glm::vec3(1.0f);
    SHRGB changed = calm;
    changed.coefficients[0] = glm::vec3(5.0f);
    const std::vector<glm::vec3> positions = {
        glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f)};

    const auto importance = ParticleProbeScattering::estimateImportance(
        positions, {changed, calm}, {calm, calm}, 0.0f);
    ASSERT_EQ(2U, importance.size());
    EXPECT_GT(importance[0], importance[1]);
}

TEST(ParticleProbeScatteringTest, ImportanceTracksSpatialGradient)
{
    SHRGB dark;
    dark.degree = 0;
    dark.coefficients[0] = glm::vec3(0.0f);
    SHRGB bright = dark;
    bright.coefficients[0] = glm::vec3(4.0f);
    const std::vector<glm::vec3> positions = {
        glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(10.0f, 0.0f, 0.0f)};

    const auto importance = ParticleProbeScattering::estimateImportance(
        positions, {dark, bright, dark}, {}, 2.0f);
    ASSERT_EQ(3U, importance.size());
    EXPECT_GT(importance[0], importance[2]);
    EXPECT_GT(importance[1], importance[2]);
}

TEST(ParticleProbeScatteringTest, InterpolationIsNormalized)
{
    ParticleProbe left;
    left.position = glm::vec3(-1.0f, 0.0f, 0.0f);
    left.valid = true;
    left.radiance.degree = 0;
    left.radiance.coefficients[0] = glm::vec3(2.0f);
    ParticleProbe right = left;
    right.position = glm::vec3(1.0f, 0.0f, 0.0f);
    right.radiance.coefficients[0] = glm::vec3(6.0f);

    const auto result = ParticleProbeScattering::interpolate(
        {glm::vec3(0.0f)}, {left, right}, 10.0f);
    EXPECT_NEAR(4.0f, result[0].coefficients[0].x, 1.0e-5f);
}

TEST(ParticleProbeScatteringTest, HistoryResetsWhenProbeMovesTooFar)
{
    ParticleProbe current;
    current.position = glm::vec3(10.0f, 0.0f, 0.0f);
    current.valid = true;
    current.radiance.degree = 0;
    current.radiance.coefficients[0] = glm::vec3(10.0f);
    ParticleProbe previous = current;
    previous.position = glm::vec3(0.0f);
    previous.radiance.coefficients[0] = glm::vec3(1.0f);

    const auto result = ParticleProbeScattering::updateHistory({current}, {previous}, 0.9f, 1.0f);
    EXPECT_FLOAT_EQ(10.0f, result[0].radiance.coefficients[0].x);
}

TEST(ParticleProbeScatteringTest, ScatteringOrderProducesAProbeCache)
{
    SHRGB sourceRadiance;
    sourceRadiance.degree = 1;
    sourceRadiance.coefficients[0] = glm::vec3(4.0f);
    const std::vector<glm::vec3> positions = {
        glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f)};
    const auto result = ParticleProbeScattering::computeScatteringOrder(
        positions, {sourceRadiance, sourceRadiance}, {1}, {1.0f, 1.0f}, 2.0f, 0.0f, 1);

    ASSERT_EQ(1U, result.size());
    EXPECT_TRUE(result[0].valid);
    EXPECT_EQ(1, result[0].radiance.degree);
    EXPECT_GT(result[0].radiance.coefficients[0].x, 0.0f);
}

TEST(ParticleProbeScatteringTest, InvalidProbeIndexDoesNotThrow)
{
    SHRGB sourceRadiance;
    sourceRadiance.degree = 0;
    sourceRadiance.coefficients[0] = glm::vec3(1.0f);
    const auto result = ParticleProbeScattering::computeScatteringOrder(
        {glm::vec3(0.0f)}, {sourceRadiance}, {99}, {1.0f}, 1.0f, 0.0f, 0);

    ASSERT_EQ(1U, result.size());
    EXPECT_FALSE(result[0].valid);
}

TEST(ParticleProbeScatteringTest, SolveAccumulatesAdditionalScatteringOrders)
{
    SHRGB direct;
    direct.degree = 0;
    direct.coefficients[0] = glm::vec3(4.0f);
    const std::vector<glm::vec3> positions = {
        glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f)};
    const auto oneOrder = ParticleProbeScattering::solve(
        positions, {direct, direct}, {0, 1}, {0.5f, 0.5f}, 2.0f, 0.0f, 0, 0);
    const auto threeOrders = ParticleProbeScattering::solve(
        positions, {direct, direct}, {0, 1}, {0.5f, 0.5f}, 2.0f, 0.0f, 2, 0);

    ASSERT_EQ(2U, oneOrder.size());
    ASSERT_EQ(2U, threeOrders.size());
    EXPECT_FLOAT_EQ(4.0f, oneOrder[0].coefficients[0].x);
    EXPECT_GT(threeOrders[0].coefficients[0].x, oneOrder[0].coefficients[0].x);
}
