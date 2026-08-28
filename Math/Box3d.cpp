#include "Box3d.h"

#include <numeric>

using namespace Phantom::Math;

template<typename T>
Box3d<T>::Box3d() :
	min(0, 0, 0),
	max(1, 1, 1)
{
}

template<typename T>
Box3d<T>::Box3d(const Vector3d<T>& point) :
	min(point),
	max(point)
{}

template<typename T>
Box3d<T>::Box3d(const Vector3d<T>& pointX, const Vector3d<T>& pointY)
{
	const auto x = std::min<T>(pointX.x, pointY.x);
	const auto y = std::min<T>(pointX.y, pointY.y);
	const auto z = std::min<T>(pointX.z, pointY.z);
	this->min = Vector3d<T>(x, y, z);
	const auto endX = std::max<T>(pointX.x, pointY.x);
	const auto endY = std::max<T>(pointX.y, pointY.y);
	const auto endZ = std::max<T>(pointX.z, pointY.z);
	this->max = Vector3d<T>(endX, endY, endZ);
	//	assert(isValid());
}

template<typename T>
Box3d<T> Box3d<T>::createDegeneratedBox()
{
	Box3d<T> b;
	constexpr auto min = std::numeric_limits<T>::lowest();
	constexpr auto max = std::numeric_limits<T>::max();
	b.max = Vector3d<T>(min, min, min);
	b.min = Vector3d<T>(max, max, max);
	return b;
}

template<typename T>
void Box3d<T>::add(const Vector3d<T>& v)
{
	const auto m = getMin();
	const auto x = std::min<T>(m.x, v.x);
	const auto y = std::min<T>(m.y, v.y);
	const auto z = std::min<T>(m.z, v.z);
	min = Vector3d<T>(x, y, z);

	const auto n = getMax();
	const auto endX = std::max<T>(n.x, v.x);
	const auto endY = std::max<T>(n.y, v.y);
	const auto endZ = std::max<T>(n.z, v.z);
	max = Vector3d<T>(endX, endY, endZ);
}

template<typename T>
void Box3d<T>::add(const Box3d<T>& b)
{
	const auto m1 = this->getMin();
	const auto m2 = b.getMin();
	const auto sx = std::min<T>(m1.x, m2.x);
	const auto sy = std::min<T>(m1.y, m2.y);
	const auto sz = std::min<T>(m1.z, m2.z);
	this->min = Vector3d<T>(sx, sy, sz);

	const auto n1 = this->getMax();
	const auto n2 = b.getMax();
	const auto ex = std::max<T>(n1.x, n2.x);
	const auto ey = std::max<T>(n1.y, n2.y);
	const auto ez = std::max<T>(n1.z, n2.z);
	this->max = Vector3d<T>(ex, ey, ez);
}

template<typename T>
Vector3d<T> Box3d<T>::getLength() const
{
	return this->max - this->min;
}

template<typename T>
Vector3d<T> Box3d<T>::getPosition(const T u, const T v, const T w) const
{
	const auto length = getLength();
	const auto x = min.x + length.x * u;
	const auto y = min.y + length.y * v;
	const auto z = min.z + length.z * w;
	return Vector3d<T>(x, y, z);
}

template<typename T>
Vector3d<T> Box3d<T>::getCenter() const
{
	return getPosition(0.5, 0.5, 0.5);
}

template<typename T>
bool Box3d<T>::contains(const Vector3d<T>& p, const T tolerance) const
{
	const auto min = this->min - tolerance;
	const auto max = this->max + tolerance;
	return
		min.x < p.x && p.x < max.x &&
		min.y < p.y && p.y < max.y &&
		min.z < p.z && p.z < max.z;
}

template<typename T>
bool Box3d<T>::contains(const Box3d<T>& p, const T tolerance) const
{
	return contains(p.getMin(), tolerance) && contains(p.getMax(), tolerance);
}

template<typename T>
bool Box3d<T>::isSame(const Box3d<T>& rhs, const T tolerance) const
{
	return areSame( this->min, rhs.min, tolerance) && areSame(this->max, rhs.max, tolerance);
}


template class Phantom::Math::Box3d<float>;
template class Phantom::Math::Box3d<double>;