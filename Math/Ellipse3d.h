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

	Vector3d<T> getNormal() const;

private:
	Vector3d<T> center;
	Vector3d<T> uvec;
	Vector3d<T> vvec;
};

using Ellipse3df = Ellipse3d<float>;
using Ellipse3dd = Ellipse3d<double>;

	}
}