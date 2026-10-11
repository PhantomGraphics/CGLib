#pragma once

#include "CGLib/Math/Line3d.h"
#include "CGLib/Math/Ray3d.h"
#include "CGLib/Math/Sphere3d.h"
#include "CGLib/Math/Vector3d.h"

#include <cmath>
#include <limits>

namespace Phantom {
	namespace Space {

		/**
		 * @brief Input checks shared by the geometric query implementations.
		 *
		 * The queries return finite values or an empty result; these predicates let each overload bail out
		 * with `{}` before a division by a zero length or an arithmetic on NaN/Inf produces a bogus hit.
		 * Only the *input* is classified here -- the arithmetic for usable input is untouched.
		 */
		namespace GeometryGuards {

			template <typename T>
			inline bool isFinite(const Math::Vector3d<T>& v)
			{
				return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
			}

			/// Finite origin and direction, and a direction that is not (numerically) zero.
			template <typename T>
			inline bool isUsable(const Math::Ray3d<T>& ray)
			{
				const auto d = ray.getDirection();
				return isFinite(ray.getOrigin()) && isFinite(d) && glm::dot(d, d) > std::numeric_limits<T>::min();
			}

			/// Finite end points that are not the same point.
			template <typename T>
			inline bool isUsable(const Math::Line3d<T>& line)
			{
				const auto d = line.getDirection();
				return isFinite(line.getStart()) && isFinite(line.getEnd()) && glm::dot(d, d) > std::numeric_limits<T>::min();
			}

			template <typename T>
			inline bool isUsable(const Math::Sphere3d<T>& sphere)
			{
				return isFinite(sphere.getCenter()) && std::isfinite(sphere.getRadius());
			}

		}
	}
}
