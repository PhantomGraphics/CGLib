#include "Circle2d.h"

#include "pi.h"

using namespace Phantom::Math;

template<typename T>
Circle2d<T>::Circle2d() :
	Circle2d(T(0.5), Vector2d<T>(0, 0))
{
}

template<typename T>
Circle2d<T>::Circle2d(const T radius, const Vector2d<T>& center) :
	radius(radius),
	center(center)
{
}

template<typename T>
Vector2d<T> Circle2d<T>::getPosition(const T u) const
{
	// u [0,1.0] 	->[0, 2pi]
	const T angle = u * T(2) * T(PI);

	const T x = center.x + radius * glm::cos(angle);
	const T y = center.y + radius * glm::sin(angle);
	return Vector2d<T>(x, y);
}

template class Phantom::Math::Circle2d<float>;
template class Phantom::Math::Circle2d<double>;
