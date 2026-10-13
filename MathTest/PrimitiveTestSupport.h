#pragma once

#include <type_traits>

// Shared tolerances for precision-parametrised primitive tests.
// Positions here have magnitude ~1..10 and go through one cos/sin + a few
// multiply-adds, so float uses 1e-5 (about 100 ulp at 1) and double 1e-12.
template<typename T>
constexpr T primitiveTolerance()
{
	return std::is_same_v<T, float> ? T(1.0e-5) : T(1.0e-12);
}

using PrimitiveScalars = ::testing::Types<float, double>;
