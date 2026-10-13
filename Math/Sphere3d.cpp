#include "Sphere3d.h"
#include "Box3d.h"
#include "SafeNormalize.h"

//#include <numeric>
#include "pi.h"

using namespace Phantom::Math;

template<typename T>
Sphere3d<T>::Sphere3d() :
	center(Vector3dd(0, 0, 0)),
	radius(1.0)
{}

template<typename T>
Sphere3d<T>::Sphere3d(const Vector3d<T>& center, const T radius) :
	center(center),
	radius(radius)
{}

template<typename T>
Vector3d<T> Sphere3d<T>::getPosition(const T u, const T v) const
{
	return getPosition(u, v, 1.0);
}

template<typename T>
Vector3d<T> Sphere3d<T>::getPosition(const T u, const T v, const T w) const
{
	const auto theta = u * PI;
	const auto phi = 2.0 * v * PI;
	const auto x = w * radius * std::sin(theta) * std::cos(phi);
	const auto y = w * radius * std::sin(theta) * std::sin(phi);
	const auto z = w * radius * std::cos(theta);
	return center + Vector3d<T>(x, y, z);
}

template<typename T>
bool Sphere3d<T>::tryGetNormal(const T u, const T v, Vector3d<T>& out) const
{
	return tryNormalize(getPosition(u, v, 1.0) - center, T(0), out);
}

template<typename T>
Vector3d<T> Sphere3d<T>::getNormal(const T u, const T v) const
{
	Vector3d<T> n(0, 0, 0);
	tryGetNormal(u, v, n);
	return n;
}

template<typename T>
Box3d<T> Sphere3d<T>::getBoundingBox() const
{
	const auto min = center - Vector3d<T>(radius, radius, radius);
	const auto max = center + Vector3d<T>(radius, radius, radius);
	return Box3d<T>(min, max);
}

template<typename T>
bool Sphere3d<T>::contains(const Vector3d<T>& p, const T tolerance) const
{
	const auto r = radius + tolerance;
	return ::getDistanceSquared(p, center) < r * r;
}


template class Phantom::Math::Sphere3d<float>;
template class Phantom::Math::Sphere3d<double>;