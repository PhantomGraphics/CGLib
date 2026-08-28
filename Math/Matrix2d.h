#pragma once

#include "glm.h"

namespace Phantom {
	namespace Math {

		template<typename T>
		using Matrix2d = glm::mat<2, 2, T>;

		using Matrix2df = Matrix2d<float>;
		using Matrix2dd = Matrix2d<double>;

		template<typename T>
		static Matrix2d<T> rotationMatrix(const T angle)
		{
			return Matrix2d<T>
				(
					::cos(angle), -::sin(angle),
					::sin(angle), ::cos(angle)
					);
		}

		template<typename T>
		static bool areSame(const Matrix2d<T>& lhs, const Matrix2d<T>& rhs, const T tolerance)
		{
			for (int i = 0; i < 2; ++i) {
				for (int j = 0; j < 2; ++j) {
					if (::fabs(lhs[i][j] - rhs[i][j]) > tolerance) {
						return false;
					}
				}
			}
			return true;
		}

	}
}