#include "Line2d.h"

namespace Phantom::Math {

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

template class Line2d<float>;
template class Line2d<double>;

} // namespace Phantom::Math
