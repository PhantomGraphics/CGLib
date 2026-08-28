#include "Circle3d.h"

using namespace Phantom::Math;

template<typename T>
Circle3d<T>::Circle3d() :
	Circle3d(T(0.5), Vector3d<T>(0,0,0), Vector3d<T>(0,0,1))
{}

template<typename T>
Circle3d<T>::Circle3d(const T radius, const Vector3d<T>& center, const Vector3d<T>& normal) :
	radius(radius),
	center(center),
	normal(normal)
{
}

template class Phantom::Math::Circle3d<float>;
template class Phantom::Math::Circle3d<double>;
