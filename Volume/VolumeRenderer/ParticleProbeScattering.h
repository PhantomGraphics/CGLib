#pragma once

#include "../../Math/SphericalHarmonics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

namespace Phantom::Volume {

struct ParticleProbe
{
    glm::vec3 position{0.0f};
    Phantom::Math::SHRGB radiance{};
    bool valid = false;
};

// CPU reference implementation for the probe pass. It intentionally contains
// no Vulkan types: the same selection, interpolation and history rules can be
// used by a GPU implementation and by the Phase-0 convergence experiment.
class ParticleProbeScattering
{
public:
    static std::vector<std::size_t> selectUniform(const std::vector<glm::vec3>& positions,
                                                  const std::size_t probeCount,
                                                  const std::uint32_t seed = 0x50425652U)
    {
        std::vector<std::size_t> indices(positions.size());
        std::iota(indices.begin(), indices.end(), std::size_t{0});
        if (probeCount >= indices.size())
            return indices;
        std::mt19937 generator(seed);
        std::shuffle(indices.begin(), indices.end(), generator);
        indices.resize(probeCount);
        std::sort(indices.begin(), indices.end());
        return indices;
    }

    // Greedy importance sampling for the adaptive Phase-3 probe layout. The
    // score is intentionally supplied by the caller so a renderer can use
    // temporal variance, spatial gradients, or both without coupling this
    // CPU utility to a particular particle representation.
    static std::vector<std::size_t> selectAdaptive(
        const std::vector<glm::vec3>& positions,
        const std::vector<float>& importance,
        const std::size_t probeCount,
        const float minimumSpacing)
    {
        if (positions.empty() || importance.size() != positions.size() || probeCount == 0)
            return {};

        std::vector<std::size_t> candidates(positions.size());
        std::iota(candidates.begin(), candidates.end(), std::size_t{0});
        std::stable_sort(candidates.begin(), candidates.end(), [&importance](const auto lhs, const auto rhs) {
            if (importance[lhs] != importance[rhs])
                return importance[lhs] > importance[rhs];
            return lhs < rhs;
        });

        const float spacingSquared = std::max(0.0f, minimumSpacing) *
            std::max(0.0f, minimumSpacing);
        std::vector<std::size_t> result;
        result.reserve(std::min(probeCount, positions.size()));
        for (const std::size_t candidate : candidates) {
            bool sufficientlyFar = true;
            if (spacingSquared > 0.0f) {
                for (const std::size_t selected : result) {
                    const glm::vec3 delta = positions[candidate] - positions[selected];
                    if (glm::dot(delta, delta) < spacingSquared) {
                        sufficientlyFar = false;
                        break;
                    }
                }
            }
            if (!sufficientlyFar)
                continue;
            result.push_back(candidate);
            if (result.size() == probeCount)
                break;
        }

        // A spacing constraint can leave the requested budget unused. Fill
        // the remainder by importance so the caller still gets a valid,
        // deterministic probe set in compact particle clouds.
        if (result.size() < std::min(probeCount, positions.size())) {
            for (const std::size_t candidate : candidates) {
                if (std::find(result.begin(), result.end(), candidate) == result.end())
                    result.push_back(candidate);
                if (result.size() == probeCount)
                    break;
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    // Estimate where the probe cache needs more samples. Temporal change is
    // measured against the previous SH field; the spatial term detects local
    // radiance boundaries even when the scene is static.
    static std::vector<float> estimateImportance(
        const std::vector<glm::vec3>& positions,
        const std::vector<Phantom::Math::SHRGB>& current,
        const std::vector<Phantom::Math::SHRGB>& previous,
        const float neighborRadius)
    {
        if (current.size() != positions.size())
            return {};

        const float radiusSquared = std::max(0.0f, neighborRadius) *
            std::max(0.0f, neighborRadius);
        std::vector<float> result(positions.size(), 1.0f);
        for (std::size_t i = 0; i < positions.size(); ++i) {
            if (previous.size() == current.size())
                result[i] += radianceDifference(current[i], previous[i]);
            if (radiusSquared <= 0.0f)
                continue;

            float gradient = 0.0f;
            for (std::size_t j = 0; j < positions.size(); ++j) {
                if (i == j)
                    continue;
                const glm::vec3 delta = positions[j] - positions[i];
                const float distanceSquared = glm::dot(delta, delta);
                if (distanceSquared <= 1.0e-10f || distanceSquared > radiusSquared)
                    continue;
                gradient = std::max(gradient,
                    radianceDifference(current[i], current[j]) / std::sqrt(distanceSquared));
            }
            result[i] += gradient;
        }
        return result;
    }

    static std::vector<Phantom::Math::SHRGB> interpolate(
        const std::vector<glm::vec3>& positions,
        const std::vector<ParticleProbe>& probes,
        const float kernelRadius)
    {
        std::vector<Phantom::Math::SHRGB> result(positions.size());
        if (probes.empty() || kernelRadius <= 0.0f)
            return result;

        const float inverseRadiusSquared = 1.0f / (kernelRadius * kernelRadius);
        for (std::size_t particle = 0; particle < positions.size(); ++particle) {
            float weightSum = 0.0f;
            for (const auto& probe : probes) {
                if (!probe.valid)
                    continue;
                const glm::vec3 delta = positions[particle] - probe.position;
                const float weight = std::exp(-glm::dot(delta, delta) * inverseRadiusSquared);
                result[particle] += probe.radiance * weight;
                weightSum += weight;
            }
            if (weightSum > 1.0e-8f)
                result[particle] = result[particle] * (1.0f / weightSum);
        }
        return result;
    }

    // Advance one scattering order. This is the grid-free CPU reference for
    // the GPU probe pass: previousOrder is stored per particle, while only
    // probeIndices receive a new environment/SH cache. The Gaussian kernel is
    // normalized per probe, so changing the probe density does not change the
    // energy of a uniform field.
    static std::vector<ParticleProbe> computeScatteringOrder(
        const std::vector<glm::vec3>& positions,
        const std::vector<Phantom::Math::SHRGB>& previousOrder,
        const std::vector<std::size_t>& probeIndices,
        const std::vector<float>& albedo,
        const float kernelRadius,
        const float phaseG,
        const int degree)
    {
        std::vector<ParticleProbe> result;
        result.reserve(probeIndices.size());
        const int clampedDegree = glm::clamp(degree, 0, 2);
        if (kernelRadius <= 0.0f || positions.empty() ||
            previousOrder.size() != positions.size())
            return result;

        const float inverseRadiusSquared = 1.0f / (kernelRadius * kernelRadius);
        for (const std::size_t probeIndex : probeIndices) {
            ParticleProbe probe;
            probe.radiance.degree = clampedDegree;
            probe.position = probeIndex < positions.size() ? positions[probeIndex] : glm::vec3(0.0f);
            if (probeIndex >= positions.size()) {
                result.push_back(probe);
                continue;
            }

            float weightSum = 0.0f;
            for (std::size_t sourceIndex = 0; sourceIndex < positions.size(); ++sourceIndex) {
                const glm::vec3 delta = positions[sourceIndex] - probe.position;
                const float distanceSquared = glm::dot(delta, delta);
                if (distanceSquared <= 1.0e-10f)
                    continue;

                const float distance = std::sqrt(distanceSquared);
                const glm::vec3 toSource = delta / distance;
                const glm::vec3 toProbe = -toSource;
                const float weight = std::exp(-distanceSquared * inverseRadiusSquared);
                const glm::vec3 radiance = Phantom::Math::evaluate(
                    previousOrder[sourceIndex], toProbe, false);
                const float sourceAlbedo = sourceIndex < albedo.size()
                    ? std::max(0.0f, albedo[sourceIndex]) : 1.0f;
                weightSum += weight;
                for (int coefficient = 0; coefficient < (clampedDegree + 1) * (clampedDegree + 1); ++coefficient)
                    probe.radiance.coefficients[static_cast<std::size_t>(coefficient)] +=
                        radiance * (Phantom::Math::sphericalHarmonicBasis(coefficient, toSource) * weight * sourceAlbedo);
            }

            if (weightSum > 1.0e-8f) {
                probe.radiance = Phantom::Math::convolveHenyeyGreenstein(
                    probe.radiance * (1.0f / weightSum), phaseG);
                probe.valid = true;
            }
            result.push_back(probe);
        }
        return result;
    }

    // Solve a truncated Neumann series. directOrder is the first (usually
    // light-source/shadow-map) order. Each following order is evaluated only
    // at probes, interpolated back to particles, and then becomes the source
    // term for the next iteration. No voxel grid is introduced here.
    static std::vector<Phantom::Math::SHRGB> solve(
        const std::vector<glm::vec3>& positions,
        const std::vector<Phantom::Math::SHRGB>& directOrder,
        const std::vector<std::size_t>& probeIndices,
        const std::vector<float>& albedo,
        const float kernelRadius,
        const float phaseG,
        const int additionalOrders,
        const int degree)
    {
        if (positions.empty() || directOrder.size() != positions.size() ||
            additionalOrders < 0)
            return {};

        std::vector<Phantom::Math::SHRGB> total = directOrder;
        std::vector<Phantom::Math::SHRGB> current = directOrder;
        for (int order = 0; order < additionalOrders; ++order) {
            const auto probes = computeScatteringOrder(
                positions, current, probeIndices, albedo, kernelRadius, phaseG, degree);
            current = interpolate(positions, probes, kernelRadius);
            for (std::size_t i = 0; i < total.size(); ++i)
                total[i] += current[i];
        }
        return total;
    }

    static std::vector<ParticleProbe> updateHistory(
        const std::vector<ParticleProbe>& current,
        const std::vector<ParticleProbe>& previous,
        const float historyWeight,
        const float resetDistance)
    {
        std::vector<ParticleProbe> result = current;
        const float weight = std::clamp(historyWeight, 0.0f, 1.0f);
        for (std::size_t i = 0; i < result.size(); ++i) {
            if (i >= previous.size() || !previous[i].valid || !result[i].valid)
                continue;
            const float distance = glm::length(result[i].position - previous[i].position);
            if (distance > resetDistance)
                continue;
            result[i].radiance = result[i].radiance * (1.0f - weight) + previous[i].radiance * weight;
        }
        return result;
    }

private:
    static float radianceDifference(const Phantom::Math::SHRGB& lhs,
                                    const Phantom::Math::SHRGB& rhs)
    {
        const int degree = std::min(lhs.degree, rhs.degree);
        float squaredDifference = 0.0f;
        for (int i = 0; i < (degree + 1) * (degree + 1); ++i) {
            const glm::vec3 delta = lhs.coefficients[static_cast<std::size_t>(i)] -
                rhs.coefficients[static_cast<std::size_t>(i)];
            squaredDifference += glm::dot(delta, delta);
        }
        return std::sqrt(squaredDifference);
    }
};

} // namespace Phantom::Volume
