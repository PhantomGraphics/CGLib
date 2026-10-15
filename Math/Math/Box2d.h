#pragma once

#include "Vector2d.h"
//#include "IVolume3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Box2d// : public IVolume3d<T>
{
public:
	Box2d();

	explicit Box2d(const Vector2d<T>& point);

	Box2d(const Vector2d<T>& pointX, const Vector2d<T>& pointY);

	static Box2d<T> createDegeneratedBox();

	void add(const Vector2d<T>& v);

	void add(const Box2d<T>& b);

	Vector2d<T> getMax() const { return max; }

	Vector2d<T> getMin() const { return min; }

private:
	Vector2d<T> min;
	Vector2d<T> max;
};

using Box2df = Box2d<float>;
using Box2dd = Box2d<double>;

	}
}