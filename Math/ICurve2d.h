#pragma once

#include "Vector3d.h"

namespace Phantom {
	namespace Math {

		template<typename T>
		class ICurve2d
		{
		public:
			virtual ~ICurve2d() = default;

			virtual Vector2d<T> getPosition(const T u) const = 0;
		};

		using ICurve2df = ICurve2d<float>;
		using ICurve2dd = ICurve2d<double>;

	}
}