#include "Line2d.h"

using namespace Phantom::Math;

template<typename T>
Line2d<T>::Line2d() :
	start(0, 0),
	end(1, 0)
{
}

template<typename T>
Line2d<T>::Line2d(const Vector2d<T>& start, const Vector2d<T>& end) :
	start(start),
	end(end)
{
}

template class Phantom::Math::Line2d<float>;
template class Phantom::Math::Line2d<double>;