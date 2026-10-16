#pragma once

#include <vector>

namespace Phantom {
	namespace Math {
		template<typename T>
		class Line2d;
		template<typename T>
		class Circle2d;
		template<typename T>
		class Ray2d;
	}
	namespace Space {

		/**
		 * @brief Computes parametric intersections between 2D geometric primitives.
		 *
		 * All methods are static. Return values are vectors of parametric `t` values
		 * along the line/ray such that `point = origin + t * direction`.
		 * An empty vector indicates no intersection.
		 *
		 * @tparam T Floating-point type (e.g., float or double).
		 */
		template<typename T>
		class Intersection2d
		{
		public:
			/**
			 * @brief Computes the parametric intersection(s) of a 2D line with a circle.
			 * @param line      The infinite 2D line.
			 * @param circle    The 2D circle.
			 * @param tolerance Numerical tolerance (used to detect tangent cases).
			 * @return Vector of up to 2 parametric `t` values; empty if no intersection.
			 */
			static std::vector<T> calculate(const Math::Line2d<T>& line, const Math::Circle2d<T>& circle, const T tolerance);
		};
	}
}
