#pragma once

#include "Vector3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Circle3d
{
public:
	Circle3d();

	Circle3d(const T radius, const Vector3d<T>& center, const Vector3d<T>& normal);

	T getRadius() const { return radius; }

	Vector3d<T> getCenter() const { return center; }

	Vector3d<T> getNormal() const { return normal; }

private:
	T radius;
	Vector3d<T> center;
	Vector3d<T> normal;
};

using Circle3df = Circle3d<float>;
using Circle3dd = Circle3d<double>;
	}
}