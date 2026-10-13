#pragma once

#include "Vector3d.h"

#include <cmath>
#include <limits>

namespace Phantom {
	namespace Math {

/// Writes normalize(v) to `out` and returns true. Returns false and leaves `out`
/// unchanged when v is zero-length, subnormal-small relative to `scale`, or
/// non-finite. `scale` is the magnitude v is expected to have; a vector shorter
/// than a few ulps of it is treated as numerically zero.
template<typename T>
bool tryNormalize(const Vector3d<T>& v, const T scale, Vector3d<T>& out)
{
	const T length = getLength(v);
	if (!std::isfinite(length) || !std::isfinite(scale) ||
		!(length > scale * std::numeric_limits<T>::epsilon() * T(8))) {
		return false;
	}
	out = v / length;
	return true;
}

/// Unit normal of the plane spanned by a and b. False (out unchanged) when a and b
/// are zero-length, (nearly) parallel or non-finite. |cross| = |a||b|sin(angle), so
/// the parallel test is relative to |a||b| and independent of absolute scale.
template<typename T>
bool tryNormalizedCross(const Vector3d<T>& a, const Vector3d<T>& b, Vector3d<T>& out)
{
	return tryNormalize(glm::cross(a, b), getLength(a) * getLength(b), out);
}

	}
}
