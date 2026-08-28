#include "Ellipse3d.h"
#include "pi.h"

using namespace Phantom::Math;

template<typename T>
Ellipse3d<T>::Ellipse3d() :
	center(0, 0, 0),
	uvec(1, 0, 0),
	vvec(0, 1, 0)
{}

template<typename T>
Ellipse3d<T>::Ellipse3d(const Vector3d<T>& center, const Vector3d<T>& uvec, const Vector3d<T>& vvec) :
	center(center),
	uvec(uvec),
	vvec(vvec)
{
}

template<typename T>
Vector3d<T> Ellipse3d<T>::getPosition(const T u) const
{
	const auto angle = T(2) * u * T(PI);

	auto uu = std::cos(angle) * uvec;
	auto vv = std::sin(angle) * vvec;
	return center + uu + vv;
}

template<typename T>
Vector3d<T> Ellipse3d<T>::getPosition(const T u, const T v) const
{
	const auto angle = T(2) * u * T(PI);

	auto uu = std::cos(angle) * uvec * v;
	auto vv = std::sin(angle) * vvec * v;
	return center + uu + vv;
}

template<typename T>
Vector3d<T> Ellipse3d<T>::getNormal() const
{
	const auto u = glm::normalize(uvec);
	const auto v = glm::normalize(vvec);
	return glm::cross(uvec,vvec);
}

template class Phantom::Math::Ellipse3d<float>;
template class Phantom::Math::Ellipse3d<double>;