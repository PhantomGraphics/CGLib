#pragma once

#include "Vector3d.h"
#include "ISurface3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Torus3d : public ISurface3d<T>
{
public:
	Torus3d();

	Torus3d(const Vector3d<T>& center, const Vector3d<T>& normal, const T majorRadius, const T minorRadius);

	Vector3d<T> getPosition(const T u, const T v) const override;

	Vector3d<T> getCenter() const { return center; }

	Vector3d<T> getNormal() const { return normal; }

	T getMajorRadius() const { return majorRadius; }

	T getMinorRadius() const { return minorRadius; }

private:
	Vector3d<T> center;
	Vector3d<T> normal;
	T majorRadius;
	T minorRadius;
};

using Torus3df = Torus3d<float>;
using Torus3dd = Torus3d<double>;

	}
}
