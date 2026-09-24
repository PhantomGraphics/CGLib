#pragma once

#include "glm.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Phantom::Math {

// Real, orthonormal spherical harmonics in the order
//   l=0: 0, l=1: x y z, l=2: xy yz z2 xz x2-y2.
// The ordering is deliberately stable so that the coefficients can be copied
// to a GPU buffer without a conversion step.
constexpr std::size_t sphericalHarmonicCoefficientCount(const int degree)
{
    return degree < 0 ? 0U : static_cast<std::size_t>((degree + 1) * (degree + 1));
}

struct SHRGB
{
    std::array<glm::vec3, 9> coefficients{};
    int degree = 0;

    void clear()
    {
        coefficients.fill(glm::vec3(0.0f));
    }

    SHRGB& operator+=(const SHRGB& rhs)
    {
        degree = std::max(degree, rhs.degree);
        const int count = degree;
        for (int i = 0; i < (count + 1) * (count + 1); ++i)
            coefficients[static_cast<std::size_t>(i)] += rhs.coefficients[static_cast<std::size_t>(i)];
        return *this;
    }
};

inline SHRGB operator+(SHRGB lhs, const SHRGB& rhs)
{
    lhs += rhs;
    return lhs;
}

inline SHRGB operator*(const SHRGB& value, const float scale)
{
    SHRGB result = value;
    for (auto& coefficient : result.coefficients)
        coefficient *= scale;
    return result;
}

inline float sphericalHarmonicBasis(const int index, const glm::vec3& direction)
{
    constexpr float Y00 = 0.28209479177387814f;
    constexpr float Y1 = 0.4886025119029199f;
    constexpr float Y20 = 0.31539156525252005f;
    constexpr float Y21 = 1.0925484305920792f;
    constexpr float Y22 = 0.5462742152960396f;

    switch (index) {
    case 0: return Y00;
    case 1: return Y1 * direction.x;
    case 2: return Y1 * direction.y;
    case 3: return Y1 * direction.z;
    case 4: return Y21 * direction.x * direction.y;
    case 5: return Y21 * direction.y * direction.z;
    case 6: return Y20 * (3.0f * direction.z * direction.z - 1.0f);
    case 7: return Y21 * direction.x * direction.z;
    case 8: return Y22 * (direction.x * direction.x - direction.y * direction.y);
    default: return 0.0f;
    }
}

inline SHRGB projectSamples(const std::vector<glm::vec3>& directions,
                            const std::vector<glm::vec3>& values,
                            const int degree)
{
    SHRGB result;
    result.degree = glm::clamp(degree, 0, 2);
    if (directions.empty() || directions.size() != values.size())
        return result;

    const float sampleWeight = 4.0f * 3.14159265358979323846f /
        static_cast<float>(directions.size());
    for (std::size_t sample = 0; sample < directions.size(); ++sample) {
        const float length = glm::length(directions[sample]);
        const glm::vec3 direction = length > 1.0e-8f
            ? directions[sample] / length
            : glm::vec3(0.0f, 0.0f, 1.0f);
        for (int coefficient = 0; coefficient < (result.degree + 1) * (result.degree + 1); ++coefficient)
            result.coefficients[static_cast<std::size_t>(coefficient)] +=
                values[sample] * (sphericalHarmonicBasis(coefficient, direction) * sampleWeight);
    }
    return result;
}

// Project an equirectangular/octahedral raster whose texels are uniformly
// weighted. The PBVR implementation uses an octahedral atlas; keeping the
// conversion here makes the CPU reference and the GPU convention identical.
inline glm::vec3 octahedralDirection(const int x, const int y, const int width, const int height)
{
    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width) * 2.0f - 1.0f;
    const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height) * 2.0f - 1.0f;
    glm::vec3 direction(u, v, 1.0f - std::abs(u) - std::abs(v));
    if (direction.z < 0.0f) {
        const float oldX = direction.x;
        direction.x = (1.0f - std::abs(direction.y)) * (oldX < 0.0f ? -1.0f : 1.0f);
        direction.y = (1.0f - std::abs(oldX)) * (direction.y < 0.0f ? -1.0f : 1.0f);
    }
    const float length = glm::length(direction);
    return length > 1.0e-8f ? direction / length : glm::vec3(0.0f, 0.0f, 1.0f);
}

inline SHRGB projectOctahedralMap(const std::vector<glm::vec3>& texels,
                                  const int width,
                                  const int height,
                                  const int degree)
{
    SHRGB result;
    result.degree = glm::clamp(degree, 0, 2);
    if (width <= 0 || height <= 0 || texels.size() != static_cast<std::size_t>(width * height))
        return result;

    std::vector<glm::vec3> directions;
    directions.reserve(texels.size());
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            directions.push_back(octahedralDirection(x, y, width, height));
    return projectSamples(directions, texels, result.degree);
}

// A normalized HG phase function has SH convolution eigenvalue g^l. This is
// also the useful implementation detail of Funk--Hecke for the probe pass.
inline SHRGB convolveHenyeyGreenstein(const SHRGB& incoming, const float g)
{
    SHRGB result = incoming;
    for (int l = 0; l <= incoming.degree; ++l) {
        const float eigenvalue = std::pow(g, static_cast<float>(l));
        const int first = l * l;
        const int count = 2 * l + 1;
        for (int i = 0; i < count; ++i)
            result.coefficients[static_cast<std::size_t>(first + i)] *= eigenvalue;
    }
    return result;
}

inline void applyLanczosSigmaWindow(SHRGB& value)
{
    const int n = value.degree + 1;
    for (int l = 0; l <= value.degree; ++l) {
        const float x = static_cast<float>(l) / static_cast<float>(n);
        const float sigma = x == 0.0f ? 1.0f : std::sin(3.14159265358979323846f * x) /
            (3.14159265358979323846f * x);
        const int first = l * l;
        for (int i = first; i < (l + 1) * (l + 1); ++i)
            value.coefficients[static_cast<std::size_t>(i)] *= sigma;
    }
}

inline glm::vec3 evaluate(const SHRGB& value, const glm::vec3& direction, const bool clampNegative = true)
{
    const float length = glm::length(direction);
    const glm::vec3 normalizedDirection = length > 1.0e-8f
        ? direction / length
        : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 result(0.0f);
    for (int i = 0; i < (value.degree + 1) * (value.degree + 1); ++i)
        result += value.coefficients[static_cast<std::size_t>(i)] *
            sphericalHarmonicBasis(i, normalizedDirection);
    if (clampNegative)
        result = glm::max(result, glm::vec3(0.0f));
    return result;
}

} // namespace Phantom::Math
