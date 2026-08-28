#pragma once

#include "../Math/glm.h"

namespace Phantom {
	namespace Graphics {

template<typename T>
using ColorRGB = glm::vec<3, T>;

using ColorRGBf = ColorRGB<float>;
using ColorRGBuc = ColorRGB<glm::u8>;

/// @brief RGB(float)色をRGB(unsigned char)へ変換します。
/// @param color RGB(float)色。
/// @return RGB(unsigned char)色。
static ColorRGBuc toColorRGBuc(const ColorRGBf& color)
{
	const auto r = static_cast<unsigned char>(color.r * 255);
	const auto g = static_cast<unsigned char>(color.g * 255);
	const auto b = static_cast<unsigned char>(color.b * 255);
	return ColorRGBuc(r, g, b);
}

	}
}