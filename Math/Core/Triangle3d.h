#pragma once

#include <array>
#include "Vector3d.h"

namespace Phantom {
	namespace Math {

template <typename T>
class Triangle3d
{
public:
	Triangle3d();

	explicit Triangle3d(const std::array<Vector3d<T>, 3>& vertices);

	//Triangle3d(const Vector3dd& v1, const Vector3dd& v2, const Vector3dd& v3);

	/// Unit normal, or the zero vector for a degenerate triangle (collinear/coincident
	/// vertices or non-finite input). Use tryGetNormal() to tell the cases apart.
	Vector3d<T> getNormal() const;

	/// Writes the unit normal and returns true; false (out unchanged) when the
	/// triangle is degenerate (collinear/coincident vertices) or non-finite.
	bool tryGetNormal(Vector3d<T>& out) const;

	T getArea() const;

	std::array<Vector3d<T>, 3> getVertices() const { return vertices; }

	bool isSame(const Triangle3d<T>& rhs, const T tolerance) const;

private:
	std::array<Vector3d<T>, 3> vertices;
};

using Triangle3df = Triangle3d<float>;
using Triabgle3dd = Triangle3d<double>;

	}
}