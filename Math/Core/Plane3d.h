#pragma once

#include "Vector3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Plane3d
{
public:
	Plane3d();

	Plane3d(const Vector3d<T>& origin, const Vector3d<T>& normal);

	//Plane3d(const T d, const Vector3d<T>& normal);

	Vector3d<T> getOrigin() const { return origin; }

	Vector3d<T> getNormal() const { return normal; }

	T getDistance(const Vector3d<T>& position) const;

	//bool isSame(const Plane3d& rhs, const double tolerance) const;

	//double calculateD() const;

private:
	Vector3d<T> origin;
	Vector3d<T> normal;
};

using Plane3df = Plane3d<float>;
using Plane3dd = Plane3d<double>;

	}
}