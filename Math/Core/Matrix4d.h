#pragma once

#include "glm.h"

namespace Phantom {
	namespace Math {

		template<typename T>
		using Matrix4d = glm::mat<4, 4, T>;

		using Matrix4df = Matrix4d<float>;
		using Matrix4dd = Matrix4d<double>;

	}
}