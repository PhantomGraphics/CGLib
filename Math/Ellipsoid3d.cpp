#include "Ellipsoid3d.h"

#include "Sphere3d.h"

using namespace Phantom::Math;

template<typename T>
Ellipsoid3d<T>::Ellipsoid3d() :
	center(0, 0, 0),
	uvec(1, 0, 0),
	vvec(0, 1, 0),
	wvec(0, 0, 1)
{}

template<typename T>
Ellipsoid3d<T>::Ellipsoid3d(const Vector3d<T>& center, const Vector3d<T>& uvec, const Vector3d<T>& vvec, const Vector3d<T>& wvec) :
	center(center),
	uvec(uvec),
	vvec(vvec),
	wvec(wvec)
{
}

template<typename T>
Vector3d<T> Ellipsoid3d<T>::getPosition(const T u, const T v) const
{
	Sphere3d<T> s(Vector3d<T>(0, 0, 0), 1.0);
	const auto p = s.getPosition(u, v);
	auto uu = p.x * uvec;
	auto vv = p.y * vvec;
	auto ww = p.z * wvec;
	return center + uu + vv + ww;
}

template class Phantom::Math::Ellipsoid3d<float>;
template class Phantom::Math::Ellipsoid3d<double>;