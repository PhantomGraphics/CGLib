#include "gtest/gtest.h"

#include "../Volume/VolumeRenderer/ParticleProbeScattering.h"

#include <atomic>
#include <cmath>
#include <numeric>

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

TEST(ParticleProbeScatteringTest, NearestHistoryReuseSurvivesParticleReordering)
{
    ParticleProbe oldLeft;
    oldLeft.position = glm::vec3(-1.0f, 0.0f, 0.0f);
    oldLeft.valid = true;
    oldLeft.radiance.degree = 0;
    oldLeft.radiance.coefficients[0] = glm::vec3(2.0f);
    ParticleProbe oldRight = oldLeft;
    oldRight.position = glm::vec3(1.0f, 0.0f, 0.0f);
    oldRight.radiance.coefficients[0] = glm::vec3(8.0f);

    ParticleProbe newRight = oldRight;
    newRight.radiance.coefficients[0] = glm::vec3(4.0f);
    ParticleProbe newLeft = oldLeft;
    newLeft.radiance.coefficients[0] = glm::vec3(6.0f);
    const auto result = ParticleProbeScattering::updateHistoryNearest(
        {newRight, newLeft}, {oldLeft, oldRight}, 0.5f, 0.25f);

    ASSERT_EQ(2U, result.size());
    EXPECT_FLOAT_EQ(6.0f, result[0].radiance.coefficients[0].x);
    EXPECT_FLOAT_EQ(4.0f, result[1].radiance.coefficients[0].x);
}

TEST(ParticleProbeScatteringTest, NearestHistoryReuseResetsOutsideDistance)
{
    ParticleProbe current;
    current.position = glm::vec3(10.0f, 0.0f, 0.0f);
    current.valid = true;
    current.radiance.degree = 0;
    current.radiance.coefficients[0] = glm::vec3(9.0f);
    ParticleProbe previous = current;
    previous.position = glm::vec3(0.0f);
    previous.radiance.coefficients[0] = glm::vec3(1.0f);

    const auto result = ParticleProbeScattering::updateHistoryNearest(
        {current}, {previous}, 0.9f, 1.0f);
    EXPECT_FLOAT_EQ(9.0f, result[0].radiance.coefficients[0].x);
}

namespace {
constexpr float testPi = 3.14159265358979323846f;

SHRGB isotropicRadiance(const float radiance)
{
    SHRGB result;
    result.degree = 0;
    result.coefficients[0] = glm::vec3(radiance * std::sqrt(4.0f * testPi));
    return result;
}

ProbeScatteringSettings makeSettings(const float particleRadius, const float phaseG, const int degree)
{
    ProbeScatteringSettings settings;
    settings.particleRadius = particleRadius;
    settings.mapResolution = 16;
    settings.kernelRadius = 2.0f;
    settings.phaseG = phaseG;
    settings.degree = degree;
    return settings;
}

// A probe at the origin enclosed by a Fibonacci shell of opaque particles
// whose discs overlap, so every direction from the probe is covered.
std::vector<glm::vec3> makeEnclosingShell(const int count, const float radius)
{
    std::vector<glm::vec3> positions = {glm::vec3(0.0f)};
    const float golden = testPi * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < count; ++i) {
        const float z = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const float ring = std::sqrt(std::max(0.0f, 1.0f - z * z));
        const float phi = golden * static_cast<float>(i);
        positions.emplace_back(radius * ring * std::cos(phi), radius * ring * std::sin(phi), radius * z);
    }
    return positions;
}
}

TEST(ParticleProbeScatteringTest, ScatteringOrderProducesAProbeCache)
{
    const std::vector<glm::vec3> positions = {
        glm::vec3(-0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f)};
    const auto source = isotropicRadiance(1.0f);
    const auto result = ParticleProbeScattering::computeScatteringOrder(
        positions, {source, source}, {1}, {1.0f, 1.0f}, {1.0f, 1.0f}, makeSettings(0.2f, 0.0f, 1));

    ASSERT_EQ(1U, result.size());
    EXPECT_TRUE(result[0].valid);
    EXPECT_EQ(1, result[0].radiance.degree);
    EXPECT_GT(result[0].radiance.coefficients[0].x, 0.0f);
}

TEST(ParticleProbeScatteringTest, EnclosedProbeReproducesIsotropicRadiance)
{
    // Inside an optically thick medium of uniform isotropic radiance c, one
    // isotropic scattering at albedo a returns a*c. This pins down the 4*pi
    // quadrature, the octahedral solid angles and the albedo placement.
    const auto positions = makeEnclosingShell(400, 1.0f);
    const std::vector<SHRGB> previous(positions.size(), isotropicRadiance(3.0f));
    const std::vector<float> opacity(positions.size(), 1.0f);

    const auto unitAlbedo = ParticleProbeScattering::computeScatteringOrder(
        positions, previous, {0}, std::vector<float>(positions.size(), 1.0f), opacity,
        makeSettings(0.25f, 0.0f, 0));
    ASSERT_EQ(1U, unitAlbedo.size());
    const float expected = previous[0].coefficients[0].x;
    EXPECT_NEAR(expected, unitAlbedo[0].radiance.coefficients[0].x, 0.02f * expected);

    const auto halfAlbedo = ParticleProbeScattering::computeScatteringOrder(
        positions, previous, {0}, std::vector<float>(positions.size(), 0.5f), opacity,
        makeSettings(0.25f, 0.0f, 0));
    EXPECT_NEAR(0.5f * expected, halfAlbedo[0].radiance.coefficients[0].x, 0.01f * expected);
}

TEST(ParticleProbeScatteringTest, NearParticleOccludesFarParticle)
{
    // A dark opaque particle between the probe and a bright one blocks it;
    // a half-transparent one lets half through.
    const std::vector<glm::vec3> positions = {
        glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(2.0f, 0.0f, 0.0f)};
    const std::vector<SHRGB> radiance = {
        isotropicRadiance(0.0f), isotropicRadiance(0.0f), isotropicRadiance(1.0f)};
    const auto settings = makeSettings(0.3f, 0.0f, 0);
    const std::vector<float> albedo(3, 1.0f);

    const auto unblocked = ParticleProbeScattering::computeScatteringOrder(
        positions, radiance, {0}, albedo, {1.0f, 0.0f, 1.0f}, settings);
    const auto blocked = ParticleProbeScattering::computeScatteringOrder(
        positions, radiance, {0}, albedo, {1.0f, 1.0f, 1.0f}, settings);
    const auto halfBlocked = ParticleProbeScattering::computeScatteringOrder(
        positions, radiance, {0}, albedo, {1.0f, 0.5f, 1.0f}, settings);

    const float open = unblocked[0].radiance.coefficients[0].x;
    EXPECT_GT(open, 0.0f);
    EXPECT_NEAR(0.0f, blocked[0].radiance.coefficients[0].x, 1.0e-6f);
    EXPECT_NEAR(0.5f * open, halfBlocked[0].radiance.coefficients[0].x, 1.0e-3f * open);
}

TEST(ParticleProbeScatteringTest, ForwardScatteringKeepsPropagationDirection)
{
    // Light from a source at +x travels towards -x through the probe. With a
    // forward-peaked phase function the scattered light must keep going -x.
    const std::vector<glm::vec3> positions = {glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};
    const auto source = isotropicRadiance(1.0f);
    const auto result = ParticleProbeScattering::computeScatteringOrder(
        positions, {source, source}, {0}, {1.0f, 1.0f}, {1.0f, 1.0f}, makeSettings(0.3f, 0.8f, 1));

    ASSERT_EQ(1U, result.size());
    const glm::vec3 forward = Phantom::Math::evaluate(result[0].radiance, glm::vec3(-1.0f, 0.0f, 0.0f));
    const glm::vec3 backward = Phantom::Math::evaluate(result[0].radiance, glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_GT(forward.x, backward.x);
}

TEST(ParticleProbeScatteringTest, InvalidProbeIndexDoesNotThrow)
{
    const auto result = ParticleProbeScattering::computeScatteringOrder(
        {glm::vec3(0.0f)}, {isotropicRadiance(1.0f)}, {99}, {1.0f}, {1.0f}, makeSettings(0.3f, 0.0f, 0));

    ASSERT_EQ(1U, result.size());
    EXPECT_FALSE(result[0].valid);
}

TEST(ParticleProbeScatteringTest, SolveConvergesToAlbedoSeriesInThickMedium)
{
    // With every particle enclosed by the others the Neumann series becomes
    // c * (1 + a + a^2 + ...). Only the enclosed centre particle is checked;
    // the shell particles see open sky and legitimately receive less.
    const auto positions = makeEnclosingShell(400, 1.0f);
    const std::vector<SHRGB> direct(positions.size(), isotropicRadiance(1.0f));
    const std::vector<float> albedo(positions.size(), 0.5f);
    const std::vector<float> opacity(positions.size(), 1.0f);
    auto settings = makeSettings(0.25f, 0.0f, 0);
    settings.kernelRadius = 0.05f;

    std::vector<std::size_t> probes(positions.size());
    std::iota(probes.begin(), probes.end(), std::size_t{0});
    const auto oneOrder = ParticleProbeScattering::solve(
        positions, direct, probes, albedo, opacity, settings, 0);
    const auto twoOrders = ParticleProbeScattering::solve(
        positions, direct, probes, albedo, opacity, settings, 1);

    ASSERT_EQ(positions.size(), oneOrder.size());
    ASSERT_EQ(positions.size(), twoOrders.size());
    const float c = direct[0].coefficients[0].x;
    EXPECT_FLOAT_EQ(c, oneOrder[0].coefficients[0].x);
    EXPECT_NEAR(1.5f * c, twoOrders[0].coefficients[0].x, 0.02f * c);
}

TEST(ParticleProbeScatteringTest, DirectionalTransmittanceCountsOccludersTowardsTheLight)
{
    // Light from +y. The top particle is unshadowed, each lower one sits
    // behind one more disc of opacity 0.5; the off-axis particle is not.
    const std::vector<glm::vec3> positions = {
        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(5.0f, 0.0f, 0.0f)};
    const auto transmittance = ParticleProbeScattering::computeDirectionalTransmittance(
        positions, std::vector<float>(positions.size(), 0.5f), 0.3f, glm::vec3(0.0f, 1.0f, 0.0f));

    ASSERT_EQ(4U, transmittance.size());
    EXPECT_FLOAT_EQ(1.0f, transmittance[2]);
    EXPECT_FLOAT_EQ(0.5f, transmittance[1]);
    EXPECT_FLOAT_EQ(0.25f, transmittance[0]);
    EXPECT_FLOAT_EQ(1.0f, transmittance[3]);
}

TEST(ParticleProbeScatteringTest, SubsetGatherPreservesEnclosedRadiance)
{
    // A random subset with radius scaled by sqrt(N/M) keeps the expected
    // coverage, so an enclosed probe still sees the full isotropic field.
    const auto positions = makeEnclosingShell(2000, 1.0f);
    const std::vector<SHRGB> previous(positions.size(), isotropicRadiance(3.0f));
    const std::vector<float> albedo(positions.size(), 1.0f);
    const std::vector<float> opacity(positions.size(), 1.0f);
    auto settings = makeSettings(0.12f, 0.0f, 0);

    const auto full = ParticleProbeScattering::computeScatteringOrder(
        positions, previous, {0}, albedo, opacity, settings);
    settings.maxSourcesPerProbe = 400;
    const auto subset = ParticleProbeScattering::computeScatteringOrder(
        positions, previous, {0}, albedo, opacity, settings);
    const auto again = ParticleProbeScattering::computeScatteringOrder(
        positions, previous, {0}, albedo, opacity, settings);

    const float expected = previous[0].coefficients[0].x;
    EXPECT_NEAR(expected, full[0].radiance.coefficients[0].x, 0.02f * expected);
    EXPECT_NEAR(expected, subset[0].radiance.coefficients[0].x, 0.05f * expected);
    EXPECT_FLOAT_EQ(subset[0].radiance.coefficients[0].x, again[0].radiance.coefficients[0].x);
}

TEST(ParticleProbeScatteringTest, NegativeSHLobeDoesNotEmitNegativeLight)
{
    // A degree-1 lobe pointing away from the probe (1 + 3g cos with g = 0.85)
    // is negative towards it. Radiance is clamped at evaluation, so the
    // source must not subtract light from the probe.
    SHRGB awayLobe;
    awayLobe.degree = 1;
    awayLobe.coefficients[0] = glm::vec3(Phantom::Math::sphericalHarmonicBasis(0, glm::vec3(1.0f, 0.0f, 0.0f)));
    awayLobe.coefficients[1] = glm::vec3(0.85f * Phantom::Math::sphericalHarmonicBasis(1, glm::vec3(1.0f, 0.0f, 0.0f)));
    ASSERT_LT(Phantom::Math::evaluate(awayLobe, glm::vec3(-1.0f, 0.0f, 0.0f), false).x, 0.0f);

    const std::vector<glm::vec3> positions = {glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f)};
    const auto result = ParticleProbeScattering::computeScatteringOrder(
        positions, {isotropicRadiance(0.0f), awayLobe}, {0}, {1.0f, 1.0f}, {1.0f, 1.0f},
        makeSettings(0.3f, 0.0f, 0));
    ASSERT_EQ(1U, result.size());
    EXPECT_GE(result[0].radiance.coefficients[0].x, 0.0f);
}

TEST(ParticleProbeScatteringTest, CancelledSolveReturnsEmpty)
{
    // A background solve abandoned because newer settings arrived must stop
    // early and report nothing, so a stale result is never applied.
    const auto positions = makeEnclosingShell(200, 1.0f);
    const std::vector<SHRGB> direct(positions.size(), isotropicRadiance(1.0f));
    std::vector<std::size_t> probes(positions.size());
    std::iota(probes.begin(), probes.end(), std::size_t{0});
    std::atomic<bool> cancel{true};

    const auto result = ParticleProbeScattering::solve(
        positions, direct, probes, std::vector<float>(positions.size(), 0.9f),
        std::vector<float>(positions.size(), 1.0f), makeSettings(0.25f, 0.0f, 0), 2, &cancel);
    EXPECT_TRUE(result.empty());

    cancel = false;
    const auto completed = ParticleProbeScattering::solve(
        positions, direct, probes, std::vector<float>(positions.size(), 0.9f),
        std::vector<float>(positions.size(), 1.0f), makeSettings(0.25f, 0.0f, 0), 2, &cancel);
    EXPECT_EQ(positions.size(), completed.size());
}
