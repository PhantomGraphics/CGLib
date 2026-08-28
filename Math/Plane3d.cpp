#include "Plane3d.h"

using namespace Phantom::Math;

template<typename T>
Plane3d<T>::Plane3d() :
	origin(0, 0, 0),
	normal(0, 0, 1)
{
}

template<typename T>
Plane3d<T>::Plane3d(const Vector3d<T>& origin, const Vector3d<T>& normal) :
	origin(origin),
	normal(normal)
{
}

/*
template<typename T>
Plane3d<T>::Plane3d(const T d, const Vector3d<T>& normal) :
	normal(normal)
{
	origin = d * normal;
}
*/

template<typename T>
T Plane3d<T>::getDistance(const Vector3d<T>& position) const
{
	const auto& v = position - origin;
	return glm::dot(v, normal);
}

/*
template<typename T>
bool Plane3d<T>::isSame(const Plane3d<T>& rhs, const T tolerance) const
{
	return
		areSame(origin, rhs.origin, tolerance) &&
		areSame(normal, rhs.normal, tolerance);
}
*/

/*
double Plane3d::calculateD() const
{
	return glm::dot(origin, normal);
}
*/

template class Phantom::Math::Plane3d<float>;
template class Phantom::Math::Plane3d<double>;