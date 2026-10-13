#include "Line3d.h"

namespace Phantom::Math {

template<typename T>
Line3d<T>::Line3d() :
	start(0, 0, 0),
	end(1, 0, 0)
{
}

template<typename T>
Line3d<T>::Line3d(const Vector3d<T>& start, const Vector3d<T>& end) :
	start(start),
	end(end)
{
}

template class Line3d<float>;
template class Line3d<double>;

} // namespace Phantom::Math
