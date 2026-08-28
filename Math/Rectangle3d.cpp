#include "Rectangle3d.h"

#include "Plane3d.h"

using namespace Phantom::Math;

template<typename T>
Rectangle3d<T>::Rectangle3d() :
	Rectangle3d(Vector3d<T>(0, 0, 0), Vector3d<T>(1, 0, 0), Vector3d<T>(0, 1, 0))
{}

template<typename T>
Rectangle3d<T>::Rectangle3d(const Vector3d<T>& origin, const Vector3d<T>& uvec, const Vector3d<T>& vvec) :
	origin(origin),
	uvec(uvec),
	vvec(vvec)
{
}

template<typename T>
Vector3d<T> Rectangle3d<T>::getPosition(const T u, const T v) const
{
	return origin + uvec * u + vvec * v;
}

template<typename T>
Vector3d<T> Rectangle3d<T>::getNormal() const
{
	return glm::normalize(glm::cross(uvec, vvec));
}

template class Phantom::Math::Rectangle3d<float>;
template class Phantom::Math::Rectangle3d<double>;