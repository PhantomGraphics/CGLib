#pragma once

#include "Vector3d.h"
#include "ISurface3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Ellipsoid3d : public ISurface3d<T>
{
public:
	Ellipsoid3d();

	Ellipsoid3d(const Vector3d<T>& center, const Vector3d<T>& uvec, const Vector3d<T>& vvec, const Vector3d<T>& wvec);

	Vector3d<T> getPosition(const T u, const T v) const override;

	Vector3d<T> getCenter() const { return center; }

	Vector3d<T> getUVec() const { return uvec; }

	Vector3d<T> getVVec() const { return vvec; }

	Vector3d<T> getWVec() const { return wvec; }

private:
	Vector3d<T> center;
	Vector3d<T> uvec;
	Vector3d<T> vvec;
	Vector3d<T> wvec;
};

using Ellipsoid3df = Ellipsoid3d<float>;
using Ellipsoid3dd = Ellipsoid3d<double>;

	}
}