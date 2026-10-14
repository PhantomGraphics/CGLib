#pragma once

#include "ColorHSV.h"

namespace Phantom {
	namespace Graphics {

/// @brief 色空間変換を提供するユーティリティクラスです。
class ColorConverter
{
public:
	/// @brief HSV色をRGB(float)へ変換します。
	/// @param hsv HSV色。
	/// @return RGB色(float)。
	static ColorRGBf convertHSVtoRGBf(const ColorHSV& hsv);

	/// @brief HSV色をRGB(unsigned char)へ変換します。
	/// @param hsv HSV色。
	/// @return RGB色(unsigned char)。
	static ColorRGBuc convertHSVToRGBuc(const ColorHSV& hsv);

	/// @brief RGB(float)をHSVへ変換します。
	/// @param rgb RGB色(float)。
	/// @return HSV色。
	static ColorHSV conertRGBToHSV(const ColorRGBf& rgb)
	{
		return ColorHSV();
	}

	/// @brief RGB(unsigned char)をHSVへ変換します。
	/// @param rgb RGB色(unsigned char)。
	/// @return HSV色。
	static ColorHSV convertRGBToHSV(const ColorRGBuc& rgb)
	{
		return ColorHSV();
	}
	//static convertRGBtoHSV()
};
	}
}