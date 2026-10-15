#pragma once

#include "Vector2d.h"
#include "Vector3d.h"

namespace Phantom {
	namespace Math {

template<typename T>
class IVolume3d
{
public:
	virtual ~IVolume3d() = default;

	virtual Vector3d<T> getPosition(const T u, const T v, const T w) const = 0;
};

using IVolume3df = IVolume3d<float>;
using IVolume3dd = IVolume3d<double>;

	}
}