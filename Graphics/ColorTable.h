#pragma once

#include <vector>
#include "ColorRGBA.h"

namespace Phantom {
	namespace Graphics {

/// @brief 色配列を保持するカラーテーブルクラスです。
class ColorTable
{
public:
	/// @brief 空のカラーテーブルを作成します。
	ColorTable() {}

	/// @brief 解像度を指定してカラーテーブルを作成します。
	/// @param resolution テーブルの要素数。
	explicit ColorTable(const int resolution);

	/// @brief テーブルの解像度を取得します。
	/// @return テーブル要素数。
	int getResolution() const;

	/// @brief 指定インデックスの色を設定します。
	/// @param index インデックス。
	/// @param color 設定する色。
	void setColor(const int index, const Graphics::ColorRGBAf& color);

	/// @brief 指定インデックスの色を取得します。
	/// @param i インデックス。
	/// @return 取得した色。
	Graphics::ColorRGBAf getColor(const int i) const;

	/// @brief 全色配列を取得します。
	/// @return 色配列。
	std::vector< ColorRGBAf > getColors() const { return colors; }

	//static ColorTable createDefaultTable(const int resolution);

	/// @brief Jetカラーマップのテーブルを生成します。
	/// @param resolution テーブルの要素数。
	/// @return 生成されたカラーテーブル。
	static ColorTable createJetTable(const int resolution);

private:
	std::vector<ColorRGBAf> colors;
};

	}
}