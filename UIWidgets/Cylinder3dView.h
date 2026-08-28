#pragma once

#include "IView.h"
#include "Vector3dView.h"

#include "../Math/Cylinder3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Cylinder3df（3D円柱）を表示・編集するウィジェット．
 *
 * IView を継承し，底面中心点（bottom），U軸（uvec），V軸（vvec），W軸（wvec）の
 * 4つの Vector3dView を持つ．U/V 軸が底面の半径方向，W 軸が軸方向に対応する．
 */
class Cylinder3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Cylinder3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Cylinder3df）．
	 */
	Cylinder3dView(const std::string& name, const Math::Cylinder3df& value);

	/**
	 * @brief 現在の Cylinder3df 値を返す．
	 * @return bottom / uvec / vvec / wvec の各サブビューから再構成した値．
	 */
	Math::Cylinder3df getValue() const;

	/**
	 * @brief Cylinder3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Cylinder3df& value);

private:
	Vector3dView bottomView; ///< 円柱の底面中心点．
	Vector3dView uvecView;   ///< 底面の U 軸ベクトル（半径方向）．
	Vector3dView vvecView;   ///< 底面の V 軸ベクトル（半径方向）．
	Vector3dView wvecView;   ///< 円柱の軸方向ベクトル（高さ方向）．
};

	}
}
