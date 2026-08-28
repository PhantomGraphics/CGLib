#include "Cone3d.h"
#include "pi.h"

using namespace Phantom::Math;

template<typename T>
Cone3d<T>::Cone3d() :
	Cone3d(Vector3d<T>(0,0,0), Vector3d<T>(1,0,0), Vector3d<T>(0,1,0), Vector3d<T>(0,0,1))
{}

template<typename T>
Cone3d<T>::Cone3d(const Vector3d<T>& bottom, const Vector3d<T>& uvec, const Vector3d<T>& vvec, const Vector3d<T>& wvec) :
	bottom(bottom),
	uvec(uvec),
	vvec(vvec),
	wvec(wvec)
{}

template<typename T>
Vector3d<T> Cone3d<T>::getPosition(const T u, const T v) const
{
	const auto angle = T(2) * u * T(PI);
	const auto radiusScale = T(1) - v;

	const auto uu = std::cos(angle) * radiusScale * uvec;
	const auto vv = std::sin(angle) * radiusScale * vvec;
	const auto ww = v * wvec;
	return bottom + uu + vv + ww;
}

template class Phantom::Math::Cone3d<float>;
template class Phantom::Math::Cone3d<double>;
