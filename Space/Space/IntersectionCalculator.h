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
		class Rectangle3d;
		template<typename T>
		class Box3d;
	}
	namespace Space {

		/**
		 * @brief Computes parametric intersections between 3D geometric primitives.
		 *
		 * All methods are static. Return values are vectors of parametric `t` values
		 * along the ray or line such that `point = origin + t * direction`.
		 * An empty vector indicates no intersection.
		 *
		 * For sphere-plane intersection the return vector contains the signed distances
		 * of the intersection circle (by convention).
		 *
		 * @tparam T Floating-point type (e.g., float or double).
		 */
		template<typename T>
		class IntersectionCalculator
		{
		public:
			/**
			 * @brief Computes parametric intersection(s) of a ray with a sphere.
			 * @param ray       The ray.
			 * @param sphere    The sphere.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of up to 2 `t` values (entry and exit); empty if no hit.
			 */
			static std::vector<T> calculate(const Math::Ray3d<T>& ray, const Math::Sphere3d<T>& sphere, const T tolerance);

			/**
			 * @brief Computes the parametric intersection of a ray with a plane.
			 * @param ray       The ray.
			 * @param plane     The infinite plane.
			 * @param tolerance Numerical tolerance (used to detect parallel rays).
			 * @return Vector of 0 or 1 `t` values; empty if ray is parallel to the plane.
			 */
			static std::vector<T> calculate(const Math::Ray3d<T>& ray, const Math::Plane3d<T>& plane, const T tolerance);

			/**
			 * @brief Computes the parametric intersection of a ray with a triangle.
			 *
			 * Uses the Mﾃｶller窶典rumbore algorithm.
			 *
			 * @param ray       The ray.
			 * @param triangle  The triangle.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of 0 or 1 `t` values; empty if no intersection.
			 */
			static std::vector<T> calculate(const Math::Ray3d<T>& ray, const Math::Triangle3d<T>& triangle, const T tolerance);

			/**
			 * @brief Computes the parametric intersection of a ray with a rectangle (quad).
			 * @param ray       The ray.
			 * @param quad      The rectangle in 3D space.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of 0 or 1 `t` values.
			 */
			static std::vector<T> calculate(const Math::Ray3d<T>& ray, const Math::Rectangle3d<T>& quad, const T tolerance);

			/**
			 * @brief Computes the parametric intersections of a ray with an AABB.
			 * @param ray       The ray.
			 * @param box       The axis-aligned bounding box.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of up to 2 `t` values (slab entry and exit); empty if no hit.
			 */
			static std::vector<T> calculate(const Math::Ray3d<T>& ray, const Math::Box3d<T>& box, const T tolerance);

			/**
			 * @brief Computes the parametric intersection of a line with a plane.
			 * @param line      The infinite line.
			 * @param plane     The infinite plane.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of 0 or 1 `t` values.
			 */
			static std::vector<T> calculate(const Math::Line3d<T>& line, const Math::Plane3d<T> plane, const T tolerance);

			/**
			 * @brief Computes the parametric intersection(s) of a line with a sphere.
			 * @param line      The infinite line.
			 * @param sphere    The sphere.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of up to 2 `t` values.
			 */
			static std::vector<T> calculate(const Math::Line3d<T>& line, const Math::Sphere3d<T>& sphere, const T tolerance);

			/**
			 * @brief Computes the parametric intersection of a line with a triangle.
			 * @param line      The infinite line.
			 * @param triangle  The triangle.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of 0 or 1 `t` values.
			 */
			static std::vector<T> calculate(const Math::Line3d<T>& line, const Math::Triangle3d<T>& triangle, const T tolerance);

			/**
			 * @brief Computes the intersection of a sphere with a plane.
			 *
			 * Returns the signed distance values describing the intersection circle,
			 * or an empty vector if the sphere does not intersect the plane.
			 *
			 * @param sphere    The sphere.
			 * @param plane     The infinite plane.
			 * @param tolerance Numerical tolerance.
			 * @return Vector of parametric values describing the intersection; empty if none.
			 */
			static std::vector<T> calculate(const Math::Sphere3d<T>& sphere, const Math::Plane3d<T>& plane, const T tolerance);

		private:
		};
	}
}
