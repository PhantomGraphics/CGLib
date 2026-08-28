#pragma once

#include "Vector3d.h"
#include "ISurface3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Cylinder3d : public ISurface3d<T>
{
public:
	Cylinder3d();

	Cylinder3d(const Vector3d<T>& bottom, const Vector3d<T>& uvec, const Vector3d<T>& vvec, const Vector3d<T>& wvec);

	Vector3d<T> getPosition(const T u, const T v) const override;

	Vector3d<T> getBottom() const { return bottom; }

	Vector3d<T> getUVec() const { return uvec; }

	Vector3d<T> getVVec() const { return vvec; }

	Vector3d<T> getWVec() const { return wvec; }

private:
	Vector3d<T> bottom;
	Vector3d<T> uvec;
	Vector3d<T> vvec;
	Vector3d<T> wvec;
};

using Cylinder3df = Cylinder3d<float>;
using Cylinder3dd = Cylinder3d<double>;

	}
}