#pragma once

#include "glm.h"
#include <vector>

namespace Phantom {
	namespace Math {

		template<typename T>
		using Vector3d = glm::vec<3, T>;

		using Vector3df = Vector3d<float>;
		using Vector3dd = Vector3d<double>;

		using Vector3dfVector = std::vector<Vector3df>;
		using Vector3ddVector = std::vector<Vector3dd>;

		template<typename T>
		static T getLengthSquared(const Vector3d<T>& v)
		{
			return v.x * v.x + v.y * v.y + v.z * v.z;
		}

		template<typename T>
		static T getLength(const Vector3d<T>& v)
		{
			return std::sqrt(getLengthSquared(v));
		}

		template<typename T>
		static T getDistanceSquared(const Vector3d<T>& lhs, const Vector3d<T>& rhs)
		{
			const auto v = rhs - lhs;
			return glm::dot(v, v);
		}

		template<typename T>
		static T getDistance(const Vector3d<T>& lhs, const Vector3d<T>& rhs)
		{
			return std::sqrt(getDistanceSquared(lhs, rhs));
		}

		template<typename T>
		static bool areSame(const Vector3d<T>& lhs, const Vector3d<T>& rhs, const T tolerance)
		{
			return getDistanceSquared(lhs, rhs) < tolerance * tolerance;
		}

	}
}