#pragma once

#include "Vector2d.h"
#include "ICurve2d.h"

namespace Phantom {
	namespace Math {

		template<typename T>
		class Circle2d : public ICurve2d<T>
		{
		public:
			Circle2d();

			Circle2d(const T radius, const Vector2d<T>& center);

			T getRadius() const { return radius; }

			Vector2d<T> getCenter() const { return center; }

			void setCenter(const Vector2d<T>& c) { center = c; }

			void setRadius(const T r) { radius = r; }

			Vector2d<T> getPosition(const T u) const override;

		private:
			T radius;
			Vector2d<T> center;
		};

		using Circle2df = Circle2d<float>;
		using Circle2dd = Circle2d<double>;
	}
}