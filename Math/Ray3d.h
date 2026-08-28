#pragma once

#include "Vector3d.h"

namespace Phantom {
	namespace Math {

		template<typename T>
		class Ray3d
		{
		public:
			Ray3d() :
				origin(0, 0, 0),
				direction(1, 0, 0)
			{
			}

			Ray3d(const Vector3d<T>& origin, const Vector3d<T>& direction) :
				origin(origin),
				direction(direction)
			{
			}

			Vector3d<T> getOrigin() const { return origin; }

			Vector3d<T> getDirection() const { return direction; }

			Vector3d<T> getPosition(const double param) const { return origin + direction * param; }

		private:
			Vector3dd origin;
			Vector3dd direction;
		};

		using Ray3df = Ray3d<float>;
		using Ray3dd = Ray3d<double>;


	}
}