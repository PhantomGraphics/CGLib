#pragma once

#include "ColorRGB.h"
#include <cassert>

namespace Phantom {
	namespace Graphics {

/// @brief HSV色を保持するクラスです。
class ColorHSV
{
public:
	/// @brief HSV(0,0,0)で初期化します。
	ColorHSV() :
		h(0.0),
		s(0.0),
		v(0.0)
	{}

	/// @brief HSV値を指定して初期化します。
	/// @param h 色相。
	/// @param s 彩度。
	/// @param v 明度。
	ColorHSV(const float h, const float s, const float v) :
		h(h),
		s(s),
		v(v)
	{}

	/// @brief 色相を取得します。
	/// @return 色相。
	float getH() const { return h; }

	/// @brief 彩度を取得します。
	/// @return 彩度。
	float getS() const { return s; }

	/// @brief 明度を取得します。
	/// @return 明度。
	float getV() const { return v; }

	/// @brief 色相を設定します。
	/// @param h 色相。
	void setH(const float h) { this->h = h; }

	/// @brief 彩度を設定します。
	/// @param s 彩度。
	void setS(const float s) { this->s = s; }

	/// @brief 明度を設定します。
	/// @param v 明度。
	void setV(const float v) { this->v = v; }

private:
	float h;
	float s;
	float v;
};

	}
}