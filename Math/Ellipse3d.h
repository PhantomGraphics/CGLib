#pragma once

#include "Vector3d.h"
#include "ICurve3d.h"
#include "ISurface3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Ellipse3d : public ICurve3d<T>, public ISurface3d<T>
{
public:
	Ellipse3d();

	Ellipse3d(const Vector3d<T>& center, const Vector3d<T>& uvec, const Vector3d<T>& vvec);

	Vector3d<T> getPosition(const T u) const override;

	Vector3d<T> getPosition(const T u, const T v) const override;

	Vector3d<T> getCenter() const { return center; }

	Vector3d<T> getUVec() const { return uvec; }

	Vector3d<T> getVVec() const { return vvec; }

	/// Unit normal of the ellipse plane, normalize(cross(uvec, vvec)).
	/// For a degenerate ellipse (zero-length or parallel axes, non-finite values)
	/// returns the zero vector; use tryGetNormal() to detect that case.
	Vector3d<T> getNormal() const;

	/// Writes the unit normal to `out` and returns true. Returns false and leaves
	/// `out` unchanged when the axes are zero-length, (nearly) parallel or non-finite.
	bool tryGetNormal(Vector3d<T>& out) const;

private:
	Vector3d<T> center;
	Vector3d<T> uvec;
	Vector3d<T> vvec;
};

using Ellipse3df = Ellipse3d<float>;
using Ellipse3dd = Ellipse3d<double>;

	}
}