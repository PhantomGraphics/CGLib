#pragma once

#include "../../Math/SphericalHarmonics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <unordered_map>
#include <vector>

namespace Phantom::Volume {

struct ParticleProbe
{
    glm::vec3 position{0.0f};
    Phantom::Math::SHRGB radiance{};
    bool valid = false;
};

struct ProbeScatteringSettings
{
    // World-space radius of the disc each particle blocks/emits over. The
    // expected extinction of the particle medium is n * opacity * pi * r^2.
    float particleRadius = 0.5f;
    // Octahedral environment map resolution per probe (plan Sec. 3.2 step 3).
    int mapResolution = 16;
    // Width of the Gaussian kernel that spreads probe results to particles.
    float kernelRadius = 1.0f;
    float phaseG = 0.0f;
    int degree = 1;
    // ISM-style subset rendering (plan Sec. 1, contribution 1): each probe
    // gathers from an independent random subset of at most this many
    // particles, whose disc radius is scaled by sqrt(N / subset) so the
    // expected coverage of every direction is unchanged. 0 uses all.
    std::size_t maxSourcesPerProbe = 0;
    std::uint32_t seed = 0x50425652U;
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
        const auto particleCount = static_cast<std::int64_t>(positions.size());
#pragma omp parallel for schedule(static)
        for (std::int64_t signedParticle = 0; signedParticle < particleCount; ++signedParticle) {
            const auto particle = static_cast<std::size_t>(signedParticle);
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

    // Advance one scattering order: the grid-free CPU reference for the GPU
    // probe pass (plan Sec. 3.2 step 3). previousOrder is the outgoing radiance
    // stored per particle, indexed by propagation direction. For every probe
    // the other particles are splatted front to back into an octahedral map
    // as discs of settings.particleRadius with the given opacity, so nearer
    // particles occlude farther ones exactly like depth-tested PBVR particles
    // do in expectation. The map is projected to SH, converted to propagation
    // direction, convolved with the HG phase function and multiplied by the
    // albedo of the probe particle. Rays that leave the medium see no radiance.
    static std::vector<ParticleProbe> computeScatteringOrder(
        const std::vector<glm::vec3>& positions,
        const std::vector<Phantom::Math::SHRGB>& previousOrder,
        const std::vector<std::size_t>& probeIndices,
        const std::vector<float>& albedo,
        const std::vector<float>& opacity,
        const ProbeScatteringSettings& settings)
    {
        const int degree = glm::clamp(settings.degree, 0, 2);
        const int resolution = std::max(1, settings.mapResolution);
        const std::size_t texelCount = static_cast<std::size_t>(resolution * resolution);
        if (settings.particleRadius <= 0.0f || positions.empty() ||
            previousOrder.size() != positions.size())
            return {};

        std::vector<glm::vec3> texelDirections;
        texelDirections.reserve(texelCount);
        for (int y = 0; y < resolution; ++y)
            for (int x = 0; x < resolution; ++x)
                texelDirections.push_back(Phantom::Math::octahedralDirection(x, y, resolution, resolution));
        const std::vector<float> texelSolidAngles =
            Phantom::Math::octahedralTexelSolidAngles(resolution, resolution);

        constexpr float pi = 3.14159265358979323846f;
        const std::size_t sourceCount = settings.maxSourcesPerProbe == 0
            ? positions.size()
            : std::min(positions.size(), settings.maxSourcesPerProbe);
        const bool subsample = sourceCount < positions.size();
        const float radius = settings.particleRadius * (subsample
            ? std::sqrt(static_cast<float>(positions.size()) / static_cast<float>(sourceCount))
            : 1.0f);

        // Scratch buffers of one worker thread.
        struct Scratch
        {
            std::vector<std::pair<float, std::size_t>> sources;
            std::vector<glm::vec3> map;
            std::vector<float> transmittance;
            std::vector<std::size_t> indices;
        };

        const auto gather = [&](const std::size_t probeIndex, Scratch& scratch) {
            ParticleProbe probe;
            probe.radiance.degree = degree;
            if (probeIndex >= positions.size())
                return probe;
            probe.position = positions[probeIndex];

            auto& sources = scratch.sources;
            sources.clear();
            const auto addSource = [&](const std::size_t index) {
                sources.emplace_back(glm::length(positions[index] - probe.position), index);
            };
            if (subsample) {
                // Partial Fisher-Yates: a uniform subset without replacement,
                // drawn independently (but reproducibly) for every probe.
                std::mt19937 generator(settings.seed ^
                    static_cast<std::uint32_t>(probeIndex * 0x9E3779B9U));
                auto& indices = scratch.indices;
                indices.resize(positions.size());
                std::iota(indices.begin(), indices.end(), std::size_t{0});
                for (std::size_t j = 0; j < sourceCount; ++j) {
                    const std::size_t swapWith =
                        std::uniform_int_distribution<std::size_t>(j, indices.size() - 1)(generator);
                    std::swap(indices[j], indices[swapWith]);
                    addSource(indices[j]);
                }
            } else {
                for (std::size_t index = 0; index < positions.size(); ++index)
                    addSource(index);
            }
            std::sort(sources.begin(), sources.end());

            auto& map = scratch.map;
            auto& transmittance = scratch.transmittance;
            map.assign(texelCount, glm::vec3(0.0f));
            transmittance.assign(texelCount, 1.0f);
            std::size_t splatted = 0;
            for (const auto& [distance, sourceIndex] : sources) {
                if (sourceIndex == probeIndex || distance <= 1.0e-6f)
                    continue;
                const float sourceOpacity = sourceIndex < opacity.size()
                    ? glm::clamp(opacity[sourceIndex], 0.0f, 1.0f) : 1.0f;
                if (sourceOpacity <= 0.0f)
                    continue;

                const glm::vec3 toSource = (positions[sourceIndex] - probe.position) / distance;
                // Radiance is non-negative (plan Sec. 3.4: clamp at evaluation).
                // A low-order SH of a forward-peaked lobe (g = 0.85 gives
                // 1 + 3g cos < 0 backwards) would otherwise emit negative
                // light that cancels the real contributions.
                const glm::vec3 radiance = Phantom::Math::evaluate(
                    previousOrder[sourceIndex], -toSource, true);
                // A disc that contains the probe covers the facing hemisphere.
                const float cosHalfAngle = distance > radius
                    ? std::sqrt(1.0f - (radius * radius) / (distance * distance))
                    : 0.0f;
                const float discSolidAngle = 2.0f * pi * (1.0f - cosHalfAngle);
                const glm::ivec2 centreTexel = Phantom::Math::octahedralTexel(toSource, resolution, resolution);
                const std::size_t centre = static_cast<std::size_t>(centreTexel.y * resolution + centreTexel.x);

                if (discSolidAngle < texelSolidAngles[centre]) {
                    // Sub-texel particle: fractional coverage keeps the
                    // expected transmittance of the texel correct.
                    const float coverage = sourceOpacity * discSolidAngle / texelSolidAngles[centre];
                    map[centre] += radiance * (transmittance[centre] * coverage);
                    transmittance[centre] *= 1.0f - coverage;
                } else {
                    for (std::size_t texel = 0; texel < texelCount; ++texel) {
                        if (glm::dot(texelDirections[texel], toSource) < cosHalfAngle)
                            continue;
                        map[texel] += radiance * (transmittance[texel] * sourceOpacity);
                        transmittance[texel] *= 1.0f - sourceOpacity;
                    }
                }

                // Stop once every direction is (numerically) fully occluded.
                if ((++splatted & 63U) == 0U &&
                    *std::max_element(transmittance.begin(), transmittance.end()) < 1.0e-4f)
                    break;
            }

            // The map is indexed by the direction the light comes from; the
            // SH field is indexed by propagation direction (the opposite), so
            // the odd bands change sign.
            probe.radiance = Phantom::Math::projectWeightedSamples(
                texelDirections, map, texelSolidAngles, degree);
            for (int i = 1; i < 4 && i < (degree + 1) * (degree + 1); ++i)
                probe.radiance.coefficients[static_cast<std::size_t>(i)] *= -1.0f;
            const float probeAlbedo = probeIndex < albedo.size()
                ? std::max(0.0f, albedo[probeIndex]) : 1.0f;
            probe.radiance = Phantom::Math::convolveHenyeyGreenstein(probe.radiance, settings.phaseG) *
                probeAlbedo;
            probe.valid = true;
            return probe;
        };

        // Probes are independent; each worker keeps its own scratch buffers.
        // Built without OpenMP the pragmas are ignored and this runs serially.
        std::vector<ParticleProbe> result(probeIndices.size());
        const auto probeCount = static_cast<std::int64_t>(probeIndices.size());
#pragma omp parallel
        {
            Scratch scratch;
#pragma omp for schedule(dynamic, 4)
            for (std::int64_t k = 0; k < probeCount; ++k)
                result[static_cast<std::size_t>(k)] = gather(probeIndices[static_cast<std::size_t>(k)], scratch);
        }
        return result;
    }

    // Transmittance from each particle towards a directional light through
    // the same disc medium the probe maps use (plan Sec. 3.2 step 1). Discs
    // are rasterized into a grid perpendicular to the light, closest to the
    // light first; a particle reads its cell before depositing itself, so it
    // does not shadow itself.
    static std::vector<float> computeDirectionalTransmittance(
        const std::vector<glm::vec3>& positions,
        const std::vector<float>& opacity,
        const float particleRadius,
        const glm::vec3& towardsLight)
    {
        std::vector<float> result(positions.size(), 1.0f);
        const float lightLength = glm::length(towardsLight);
        if (positions.empty() || particleRadius <= 0.0f || lightLength <= 1.0e-8f)
            return result;

        const glm::vec3 axis = towardsLight / lightLength;
        const glm::vec3 helper = std::abs(axis.y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent = glm::normalize(glm::cross(helper, axis));
        const glm::vec3 bitangent = glm::cross(axis, tangent);

        std::vector<std::size_t> order(positions.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::sort(order.begin(), order.end(), [&positions, &axis](const auto lhs, const auto rhs) {
            return glm::dot(positions[lhs], axis) > glm::dot(positions[rhs], axis);
        });

        // Cells of half a radius resolve a disc with ~12 cells.
        const float cellSize = 0.5f * particleRadius;
        const int reach = static_cast<int>(std::ceil(particleRadius / cellSize));
        const auto key = [](const int x, const int y) {
            return (static_cast<std::int64_t>(x) << 32) ^ static_cast<std::uint32_t>(y);
        };
        std::unordered_map<std::int64_t, float> cells;
        cells.reserve(positions.size() * 4);
        for (const std::size_t index : order) {
            const float u = glm::dot(positions[index], tangent) / cellSize;
            const float v = glm::dot(positions[index], bitangent) / cellSize;
            const int cx = static_cast<int>(std::floor(u));
            const int cy = static_cast<int>(std::floor(v));
            const auto found = cells.find(key(cx, cy));
            result[index] = found == cells.end() ? 1.0f : found->second;

            const float alpha = index < opacity.size() ? glm::clamp(opacity[index], 0.0f, 1.0f) : 1.0f;
            if (alpha <= 0.0f)
                continue;
            const float radiusInCells = particleRadius / cellSize;
            for (int y = cy - reach; y <= cy + reach; ++y) {
                for (int x = cx - reach; x <= cx + reach; ++x) {
                    const float du = static_cast<float>(x) + 0.5f - u;
                    const float dv = static_cast<float>(y) + 0.5f - v;
                    if (du * du + dv * dv > radiusInCells * radiusInCells)
                        continue;
                    const auto inserted = cells.emplace(key(x, y), 1.0f);
                    inserted.first->second *= 1.0f - alpha;
                }
            }
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
        const std::vector<float>& opacity,
        const ProbeScatteringSettings& settings,
        const int additionalOrders)
    {
        if (positions.empty() || directOrder.size() != positions.size() ||
            additionalOrders < 0)
            return {};

        std::vector<Phantom::Math::SHRGB> total = directOrder;
        std::vector<Phantom::Math::SHRGB> current = directOrder;
        for (int order = 0; order < additionalOrders; ++order) {
            const auto probes = computeScatteringOrder(
                positions, current, probeIndices, albedo, opacity, settings);
            current = interpolate(positions, probes, settings.kernelRadius);
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

    // Lagrangian history reuse that is robust to particle-buffer reordering.
    // The CPU reference uses a linear nearest-neighbour search; a renderer can
    // replace this lookup with its spatial index without changing the EMA or
    // reset semantics.
    static std::vector<ParticleProbe> updateHistoryNearest(
        const std::vector<ParticleProbe>& current,
        const std::vector<ParticleProbe>& previous,
        const float historyWeight,
        const float resetDistance)
    {
        std::vector<ParticleProbe> result = current;
        const float weight = std::clamp(historyWeight, 0.0f, 1.0f);
        const float maxDistanceSquared = std::max(0.0f, resetDistance) *
            std::max(0.0f, resetDistance);
        for (auto& probe : result) {
            if (!probe.valid || previous.empty())
                continue;

            const ParticleProbe* nearest = nullptr;
            float nearestDistanceSquared = maxDistanceSquared;
            for (const auto& candidate : previous) {
                if (!candidate.valid)
                    continue;
                const glm::vec3 delta = probe.position - candidate.position;
                const float distanceSquared = glm::dot(delta, delta);
                if (distanceSquared <= nearestDistanceSquared) {
                    nearestDistanceSquared = distanceSquared;
                    nearest = &candidate;
                }
            }
            if (nearest == nullptr)
                continue;
            probe.radiance = probe.radiance * (1.0f - weight) + nearest->radiance * weight;
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
