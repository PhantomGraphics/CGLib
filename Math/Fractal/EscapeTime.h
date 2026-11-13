#pragma once
#include <cmath>
#include <cstdint>
#include <optional>

namespace Phantom::Math::Fractal {
struct EscapeResult {
    bool escaped;
    uint32_t iterations;
};

// Optional cancellation is checked every 64 iterations. False means cancelled.
template<class Continue>
std::optional<EscapeResult> evaluate(double real, double imaginary,
    double constantReal, double constantImaginary, uint32_t limit, Continue proceed) {
    if (!limit || limit > 4096 || !std::isfinite(real) || !std::isfinite(imaginary) ||
        !std::isfinite(constantReal) || !std::isfinite(constantImaginary)) return std::nullopt;
    for (uint32_t i = 0; i < limit; ++i) {
        if ((i & 63u) == 0 && !proceed()) return std::nullopt;
        const double nextReal = real * real - imaginary * imaginary + constantReal;
        imaginary = 2 * real * imaginary + constantImaginary;
        real = nextReal;
        // Finite inputs can overflow during squaring; these points have escaped.
        if (!std::isfinite(real) || !std::isfinite(imaginary) || real * real + imaginary * imaginary > 4)
            return EscapeResult{true, i + 1};
    }
    return EscapeResult{false, limit};
}
inline std::optional<EscapeResult> mandelbrot(double real, double imaginary, uint32_t limit = 256) {
    return evaluate(0, 0, real, imaginary, limit, [] { return true; });
}
inline std::optional<EscapeResult> julia(double real, double imaginary,
    double constantReal = -0.8, double constantImaginary = 0.156, uint32_t limit = 256) {
    return evaluate(real, imaginary, constantReal, constantImaginary, limit, [] { return true; });
}
}
