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
		 * @brief Computes distances from rays or points to 3D geometric primitives.
		 *
		 * All methods are static. For ray-primitive queries the return value is a vector
		 * of parametric `t` values along the ray such that `point = ray.origin + t * ray.direction`.
		 * An empty vector means no intersection.
		 *
		 * @tparam T Floating-point type (e.g., float or double).
		 */
		template<typename T>
		class DistanceCalculator
		{
		public:
			/**
			 * @brief Computes the closest distance from a point to a triangle.
			 * @param triangle The triangle in 3D space.
			 * @param point    The query point.
			 * @param tolerance Numerical tolerance for edge/vertex cases.
			 * @return Minimum distance from the point to the triangle.
			 */
			static T calculate(const Math::Triangle3d<T>& triangle, const Math::Vector3d<T>& point, const T tolerance);

			/**
			 * @brief Computes the parametric intersection distances of a ray with a sphere.
			 * @param ray       The ray (origin + direction).
			 * @param sphere    The sphere.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of up to 2 parametric `t` values; empty if no intersection.
			 */
			static std::vector<T> calculate(const Math::Ray3d<T>& ray, const Math::Sphere3d<T>& sphere, const T tolerance);

			/**
			 * @brief Computes the parametric intersection distance of a ray with a triangle.
			 *
			 * Uses the Mﾃｶller窶典rumbore algorithm.
			 *
			 * @param ray       The ray.
			 * @param triangle  The triangle.
			 * @param tolerance Numerical tolerance (e.g., for back-face or near-miss rejection).
			 * @return Vector of 0 or 1 parametric `t` values.
			 */
			static std::vector<T> calculate(const Math::Ray3d<T>& ray, const Math::Triangle3d<T>& triangle, const T tolerance);

			/**
			 * @brief Computes the parametric intersection distances of a ray with an AABB.
			 * @param ray       The ray.
			 * @param box       The axis-aligned bounding box.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of up to 2 parametric `t` values (entry and exit); empty if no hit.
			 */
			static std::vector<T> calculate(const Math::Ray3d<T>& ray, const Math::Box3d<T>& box, const T tolerance);
		};
	}
}
