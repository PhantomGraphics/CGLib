#pragma once

#include "CGLib/Math/Vector3d.h"
#include <vector>

namespace Phantom {
	namespace Math {
		template<typename T>
		class Line3d;
		template<typename T>
		class Sphere3d;
		template<typename T>
		class Ray3d;
		template<typename T>
		class Plane3d;
		template<typename T>
		class Triangle3d;
		template<typename T>
		class Rectancle3d;
		template<typename T>
		class Box3d;
	}
	namespace Space {

		/**
		 * @brief Computes the signed distance from a point to geometric primitives.
		 *
		 * The signed distance is negative when the point is inside the primitive
		 * and positive when outside (by convention matching implicit surface representations).
		 * This is useful for constructive solid geometry (CSG), level-set methods,
		 * and collision detection.
		 *
		 * All methods are static.
		 *
		 * @tparam T Floating-point type (e.g., float or double).
		 */
		template<typename T>
		class SignedDistanceCalculator
		{
		public:
			/**
			 * @brief Computes the signed distance from a point to a sphere.
			 *
			 * Positive outside the sphere, negative inside.
			 *
			 * @param position The query point in 3D space.
			 * @param sphere   The target sphere.
			 * @return Signed distance: positive = outside, negative = inside.
			 */
			static T calculate(const Math::Vector3d<T>& position, const Math::Sphere3d<T>& sphere);

			/**
			 * @brief Computes the signed distance from a point to a plane.
			 *
			 * Positive on the side the plane normal points toward, negative on the other.
			 *
			 * @param position The query point in 3D space.
			 * @param plane    The target plane (defined by normal and offset).
			 * @return Signed distance from the point to the plane.
			 */
			static T calculate(const Math::Vector3d<T>& position, const Math::Plane3d<T>& plane);
		};
	}
}
