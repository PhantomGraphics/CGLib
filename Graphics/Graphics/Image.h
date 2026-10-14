#pragma once

#include <vector>
#include "ColorRGBA.h"

namespace Phantom {
	namespace Graphics {

template<typename T>
/// @brief RGBA画像データを保持するテンプレートクラスです。
class Image
{
public:
	/// @brief 空の画像を初期化します。
	Image();

	/// @brief サイズを指定して画像を初期化します。
	/// @param width 画像幅。
	/// @param height 画像高さ。
	Image(const int width, const int height);

	/// @brief サイズと画素値を指定して画像を初期化します。
	/// @param width 画像幅。
	/// @param height 画像高さ。
	/// @param values RGBA画素配列。
	Image(const int width, const int height, const std::vector< T >& values);

	/// @brief 画像全体を単色で塗りつぶします。
	/// @param c 塗りつぶし色。
	void fill(const ColorRGBA<T>& c);

	/// @brief 指定座標の画素色を設定します。
	/// @param i X座標。
	/// @param j Y座標。
	/// @param c 設定する色。
	void setColor(const int i, const int j, const ColorRGBA<T>& c);

	/// @brief 指定座標の画素色を取得します。
	/// @param x X座標。
	/// @param y Y座標。
	/// @return 画素色。
	ColorRGBA<T> getColor(const int x, const int y) const;

	/// @brief 画像の生データ配列を取得します。
	/// @return RGBA画素配列。
	std::vector<T> getValues() const { return values; }

	/// @brief 画像幅を取得します。
	/// @return 幅。
	int getWidth() const { return width; }

	/// @brief 画像高さを取得します。
	/// @return 高さ。
	int getHeight() const { return height; }

	/// @brief 画像内容が同一かを比較します。
	/// @param rhs 比較対象画像。
	/// @return 同一ならtrue。
	bool isSame(const Image<T>& rhs) const;

private:
	int getIndex1d(const int x, const int y) const { return (y * width + x) * 4; }

	int width;
	int height;
	std::vector<T> values;
};

using Imageuc = Image<unsigned char>;
using Imagef = Image<float>;

	}
}