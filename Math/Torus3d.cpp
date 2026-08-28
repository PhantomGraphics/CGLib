#include "Torus3d.h"
#include "pi.h"

using namespace Phantom::Math;

template<typename T>
Torus3d<T>::Torus3d() :
	Torus3d(Vector3d<T>(0, 0, 0), Vector3d<T>(0, 1, 0), T(0.5), T(0.15))
{}

template<typename T>
Torus3d<T>::Torus3d(const Vector3d<T>& center, const Vector3d<T>& normal, const T majorRadius, const T minorRadius) :
	center(center),
	normal(glm::normalize(normal)),
	majorRadius(majorRadius),
	minorRadius(minorRadius)
{}

template<typename T>
Vector3d<T> Torus3d<T>::getPosition(const T u, const T v) const
{
	// Build an orthonormal basis (uAxis, vAxis) spanning the plane perpendicular to normal.
	const auto reference = std::abs(normal.y) < T(0.999) ? Vector3d<T>(0, 1, 0) : Vector3d<T>(1, 0, 0);
	const auto uAxis = glm::normalize(glm::cross(reference, normal));
	const auto vAxis = glm::cross(normal, uAxis);

	const auto au = T(2) * u * T(PI);
	const auto av = T(2) * v * T(PI);

	const auto radial = std::cos(au) * uAxis + std::sin(au) * vAxis;
	const auto ringPosition = center + majorRadius * radial;
	return ringPosition + minorRadius * (std::cos(av) * radial + std::sin(av) * normal);
}

template class Phantom::Math::Torus3d<float>;
template class Phantom::Math::Torus3d<double>;
