#include "Box2d.h"

#include <numeric>

using namespace Phantom::Math;

template<typename T>
Box2d<T>::Box2d() :
	min(0, 0),
	max(1, 1)
{
}

template<typename T>
Box2d<T>::Box2d(const Vector2d<T>& point) :
	min(point),
	max(point)
{}

template<typename T>
Box2d<T>::Box2d(const Vector2d<T>& pointX, const Vector2d<T>& pointY)
{
	const auto x = std::min<T>(pointX.x, pointY.x);
	const auto y = std::min<T>(pointX.y, pointY.y);
	this->min = Vector2d<T>(x, y);
	const auto endX = std::max<T>(pointX.x, pointY.x);
	const auto endY = std::max<T>(pointX.y, pointY.y);
	this->max = Vector2d<T>(endX, endY);
	//	assert(isValid());
}

template<typename T>
Box2d<T> Box2d<T>::createDegeneratedBox()
{
	Box2d<T> b;
	constexpr auto min = std::numeric_limits<T>::lowest();
	constexpr auto max = std::numeric_limits<T>::max();
	b.max = Vector2d<T>(min, min);
	b.min = Vector2d<T>(max, max);
	return b;
}

template<typename T>
void Box2d<T>::add(const Vector2d<T>& v)
{
	const auto m = getMin();
	const auto x = std::min<T>(m.x, v.x);
	const auto y = std::min<T>(m.y, v.y);
	min = Vector2d<T>(x, y);

	const auto n = getMax();
	const auto endX = std::max<T>(n.x, v.x);
	const auto endY = std::max<T>(n.y, v.y);
	max = Vector2d<T>(endX, endY);
}

template<typename T>
void Box2d<T>::add(const Box2d<T>& b)
{
	const auto m1 = this->getMin();
	const auto m2 = b.getMin();
	const auto sx = std::min<T>(m1.x, m2.x);
	const auto sy = std::min<T>(m1.y, m2.y);
	this->min = Vector2d<T>(sx, sy);

	const auto n1 = this->getMax();
	const auto n2 = b.getMax();
	const auto ex = std::max<T>(n1.x, n2.x);
	const auto ey = std::max<T>(n1.y, n2.y);
	this->max = Vector2d<T>(ex, ey);
}

template class Phantom::Math::Box2d<float>;
template class Phantom::Math::Box2d<double>;