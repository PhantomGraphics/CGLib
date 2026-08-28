#pragma once

#include "glm.h"

namespace Phantom {
	namespace Math {

		template<typename T>
		using Vector2d = glm::vec<2, T>;

		using Vector2df = Vector2d<float>;
		using Vector2dd = Vector2d<double>;

		template<typename T>
		static T getLengthSquared(const Vector2d<T>& v)
		{
			return v.x * v.x + v.y * v.y;
		}

		template<typename T>
		static T getLength(const Vector2d<T>& v)
		{
			return std::sqrt(getLengthSquared(v));
		}

		template<typename T>
		static T getDistanceSquared(const Vector2d<T>& lhs, const Vector2d<T>& rhs)
		{
			const auto v = rhs - lhs;
			return glm::dot(v, v);
		}

		template<typename T>
		static T getDistance(const Vector2d<T>& lhs, const Vector2d<T>& rhs)
		{
			return std::sqrt(getDistanceSquared(lhs, rhs));
		}

	}
}