#pragma once

#include "Vector3d.h"
#include "IVolume3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Box3d : public IVolume3d<T>
{
public:
	Box3d();

	explicit Box3d(const Vector3d<T>& point);

	Box3d(const Vector3d<T>& pointX, const Vector3d<T>& pointY);

	static Box3d<T> createDegeneratedBox();

	void add(const Vector3d<T>& v);

	void add(const Box3d<T>& b);

	Vector3d<T> getMax() const { return max; }

	Vector3d<T> getMin() const { return min; }

	Vector3d<T> getLength() const;

	Vector3d<T> getPosition(const T u, const T v, const T w) const;

	Vector3d<T> getCenter() const;

	bool contains(const Vector3d<T>& p, const T tolerance) const;

	bool contains(const Box3d<T>& p, const T tolerance) const;

	bool isSame(const Box3d<T>& rhs, const T tolerance) const;

	bool intersects(const Box3d<T>& b) const
	{
		const auto aMin = this->getMin();
		const auto aMax = this->getMax();
		const auto bMin = b.getMin();
		const auto bMax = b.getMax();

		if (aMax.x < bMin.x) return false;
		if (aMin.x > bMax.x) return false;
		if (aMax.y < bMin.y) return false;
		if (aMin.y > bMax.y) return false;
		if (aMax.z < bMin.z) return false;
		if (aMin.z > bMax.z) return false;
		return true;
	}


private:
	Vector3d<T> min;
	Vector3d<T> max;
};

using Box3df = Box3d<float>;
using Box3dd = Box3d<double>;

	}
}