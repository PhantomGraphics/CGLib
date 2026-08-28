#pragma once

#include "Vector3d.h"
#include "ISurface3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Cone3d : public ISurface3d<T>
{
public:
	Cone3d();

	Cone3d(const Vector3d<T>& bottom, const Vector3d<T>& uvec, const Vector3d<T>& vvec, const Vector3d<T>& wvec);

	Vector3d<T> getPosition(const T u, const T v) const override;

	Vector3d<T> getBottom() const { return bottom; }

	Vector3d<T> getUVec() const { return uvec; }

	Vector3d<T> getVVec() const { return vvec; }

	Vector3d<T> getWVec() const { return wvec; }

	Vector3d<T> getApex() const { return bottom + wvec; }

private:
	Vector3d<T> bottom;
	Vector3d<T> uvec;
	Vector3d<T> vvec;
	Vector3d<T> wvec;
};

using Cone3df = Cone3d<float>;
using Cone3dd = Cone3d<double>;

	}
}
