#pragma once

#include "Vector3d.h"
#include "ICurve3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class Line3d : public ICurve3d<T>
{
public:
	Line3d();

	Line3d(const Math::Vector3d<T>& start, const Math::Vector3d<T>& end);

	Vector3d<T> getStart() const { return start; }

	Vector3d<T> getEnd() const { return end; }

	Vector3d<T> getDirection() const { return end - start; }

	Vector3d<T> getPosition(const T u) const override { return start + getDirection() * u; }

	T getLength() const { return getDistance(start, end); }

private:
	Math::Vector3d<T> start;
	Math::Vector3d<T> end;
};

using Line3df = Line3d<float>;
using Line3dd = Line3d<double>;
	}
}