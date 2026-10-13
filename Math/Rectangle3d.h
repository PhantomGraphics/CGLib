#pragma once

#include "Vector3d.h"
#include "ISurface3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Rectangle3d : public ISurface3d<T>
{
public:
	Rectangle3d();

	Rectangle3d(const Vector3d<T>& origin, const Vector3d<T>& uvec, const Vector3d<T>& vvec);

	Vector3d<T> getPosition(const T u, const T v) const override;

	/// Unit normal. A degenerate rectangle yields NaN components; prefer tryGetNormal().
	Vector3d<T> getNormal() const;

	/// Writes the unit normal and returns true; false (out unchanged) when uvec and
	/// vvec are zero-length, (nearly) parallel or non-finite.
	bool tryGetNormal(Vector3d<T>& out) const;

	Vector3d<T> getUVec() const { return uvec; }

	Vector3d<T> getVVec() const { return vvec; }

private:
	Vector3d<T> origin;
	Vector3d<T> uvec;
	Vector3d<T> vvec;
};

using Rectangle3df = Rectangle3d<float>;
using Rectangle3dd = Rectangle3d<double>;

	}
}