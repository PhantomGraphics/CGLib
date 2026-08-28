#pragma once

#include "glm.h"

namespace Phantom {
	namespace Math {

		template<typename T>
		using Matrix3d = glm::mat<3, 3, T>;

		using Matrix3df = Matrix3d<float>;
		using Matrix3dd = Matrix3d<double>;

		template<typename T>
		static Matrix3d<T> rotationMatrixX(const T angle)
		{
			return Matrix3d<T>
				(
					1.0, 0.0, 0.0,
					0.0, ::cos(angle), -::sin(angle),
					0.0, ::sin(angle), ::cos(angle)
					);
		}

		template<typename T>
		static Matrix3d<T> rotationMatrixY(const T angle)
		{
			return Matrix3d<T>
				(
					::cos(angle), 0.0, ::sin(angle),
					0.0, 1.0, 0.0,
					-::sin(angle), 0.0, ::cos(angle)
					);
		}

		template<typename T>
		static Matrix3d<T> rotationMatrixZ(const T angle)
		{
			return Matrix3d<T>
				(
					::cos(angle), -::sin(angle), 0.0,
					::sin(angle), cos(angle), 0.0,
					0.0, 0.0, 1.0
					);
		}

		template<typename T>
		static Matrix3d<T> identitiyMatrix3d()
		{
			return Matrix3d<T>
				(
					1, 0, 0,
					0, 1, 0,
					0, 0, 1
					);
		}

		template<typename T>
		static bool areSame(const Matrix3d<T>& lhs, const Matrix3d<T>& rhs, const T tolerance)
		{
			for (int i = 0; i < 3; ++i) {
				for (int j = 0; j < 3; ++j) {
					if (::fabs(lhs[i][j] - rhs[i][j]) > tolerance) {
						return false;
					}
				}
			}
			return true;
		}

	}
}