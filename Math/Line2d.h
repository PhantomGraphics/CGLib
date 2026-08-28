#pragma once

#include "Vector2d.h"
#include "ICurve2d.h"

namespace Phantom {
	namespace Math {

		template<typename T>
		class Line2d : public ICurve2d<T>
		{
		public:
			Line2d();

			Line2d(const Math::Vector2d<T>& start, const Math::Vector2d<T>& end);

			Vector2d<T> getStart() const { return start; }

			Vector2d<T> getEnd() const { return end; }

			Vector2d<T> getDirection() const { return end - start; }

			Vector2d<T> getPosition(const T u) const override { return start + getDirection() * u; }

			T getLength() const { return getDistance(start, end); }

		private:
			Math::Vector2d<T> start;
			Math::Vector2d<T> end;
		};

		using Line2df = Line2d<float>;
		using Line2dd = Line2d<double>;
	}
}
