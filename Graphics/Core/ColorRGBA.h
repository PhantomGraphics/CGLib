#pragma once

#include "../../Math/Core/glm.h"

namespace Phantom {
	namespace Graphics {

template<typename T>
using ColorRGBA = glm::vec<4, T>;

using ColorRGBAf = ColorRGBA<float>;
using ColorRGBAuc = ColorRGBA<glm::u8>;

	}
}