#pragma once

#include "IView.h"

#include "Vector3dView.h"

#include "../Math/Rectangle3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Rectangle3df（3D矩形）を表示・編集するウィジェット．
 *
 * IView を継承し，原点（origin），U軸（uvec），V軸（vvec）の
 * 3つの Vector3dView を持つ．U/V 軸ベクトルの長さが矩形の各辺の長さに対応する．
 */
class Rect3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Rect3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Rectangle3df）．
	 */
	Rect3dView(const std::string& name, const Math::Rectangle3df& value);

	/**
	 * @brief 現在の Rectangle3df 値を返す．
	 * @return origin / uvec / vvec の各サブビューから再構成した値．
	 */
	Math::Rectangle3df getValue() const;

	/**
	 * @brief Rectangle3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Rectangle3df& value);

private:
	Vector3dView originView; ///< 矩形の原点（基準コーナー）．
	Vector3dView uvecView;   ///< 矩形の U 軸ベクトル（1辺の方向と長さ）．
	Vector3dView vvecView;   ///< 矩形の V 軸ベクトル（もう1辺の方向と長さ）．
};

	}
}
