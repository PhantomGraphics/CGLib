#pragma once

#include "Vector3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class ICurve3d
{
public:
	virtual ~ICurve3d() = default;

	virtual Vector3d<T> getPosition(const T u) const = 0;
};

using ICurve3df = ICurve3d<float>;
using ICurve3dd = ICurve3d<double>;

	}
}