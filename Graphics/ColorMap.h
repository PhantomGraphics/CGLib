#pragma once

#include "ColorTable.h"

namespace Phantom {
	namespace Graphics {

/// @brief スカラー値をカラーテーブルにマッピングするクラスです。
class ColorMap
{
public:
	/// @brief デフォルト設定で初期化します。
	ColorMap();

	/// @brief 値域とカラーテーブルを指定して初期化します。
	/// @param min 最小値。
	/// @param max 最大値。
	/// @param table 使用するカラーテーブル。
	ColorMap(const float min, const float max, const ColorTable& table);

	/// @brief 値を0から1の範囲に正規化します。
	/// @param value 入力値。
	/// @return 正規化値。
	float getNormalized(const float value) const;

	/// @brief 値に対応するテーブルインデックスを取得します。
	/// @param value 入力値。
	/// @return テーブルインデックス。
	int getIndex(const float value) const;

	/// @brief 値に対応する色を取得します。
	/// @param v 入力値。
	/// @return 対応色。
	Graphics::ColorRGBAf getColor(const float v) const;

	/// @brief 値に対応する補間色を取得します。
	/// @param v 入力値。
	/// @return 補間後の色。
	Graphics::ColorRGBAf getInterpolatedColor(const float v) const;

	/// @brief テーブルインデックスから値を取得します。
	/// @param i テーブルインデックス。
	/// @return 値。
	float getValueFromIndex(const int i) const;

	/// @brief 最小値を設定します。
	/// @param m 最小値。
	void setMin(const float m);

	/// @brief 最小値を取得します。
	/// @return 最小値。
	float getMin() const;

	/// @brief 最大値を設定します。
	/// @param m 最大値。
	void setMax(const float m);

	/// @brief 最大値を取得します。
	/// @return 最大値。
	float getMax() const;

	/// @brief 最小値と最大値を同時に設定します。
	/// @param min__ 最小値。
	/// @param max__ 最大値。
	void setMinMax(const float min__, const float max__);

	/// @brief 設定が有効かを判定します。
	/// @return 有効な場合はtrue。
	bool isValid();

	/// @brief 使用中のカラーテーブルを取得します。
	/// @return カラーテーブル。
	ColorTable getTable() const { return table; }

private:
	ColorTable table;

	float min_;
	float max_;
};

	}
}