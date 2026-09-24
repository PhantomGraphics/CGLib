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
};

} // namespace Phantom::Volume
