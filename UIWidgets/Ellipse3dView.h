#pragma once

#include "IView.h"
#include "Vector3dView.h"

#include "../Math/Ellipse3d.h"

namespace Phantom {
	namespace UI {

/**
 * @brief Math::Ellipse3df（3D楕円）を表示・編集するウィジェット．
 *
 * IView を継承し，中心点（center），U軸（uvec），V軸（vvec）の
 * 3つの Vector3dView を持つ．U/V 軸の長さが各半径に対応する．
 */
class Ellipse3dView : public IView
{
public:
	/**
	 * @brief コンストラクタ（初期値ゼロ）．
	 * @param name ラベル文字列．
	 */
	explicit Ellipse3dView(const std::string& name);

	/**
	 * @brief コンストラクタ（初期値指定）．
	 * @param name  ラベル文字列．
	 * @param value 初期値（Ellipse3df）．
	 */
	Ellipse3dView(const std::string& name, const Math::Ellipse3df& value);

	/**
	 * @brief 現在の Ellipse3df 値を返す．
	 * @return center / uvec / vvec の各サブビューから再構成した値．
	 */
	Math::Ellipse3df getValue() const;

	/**
	 * @brief Ellipse3df 値をプログラムから設定する．
	 * @param value 設定する値．
	 */
	void setValue(const Math::Ellipse3df& value);

private:
	Vector3dView centerView; ///< 楕円の中心点．
	Vector3dView uvecView;   ///< 楕円の U 軸ベクトル．
	Vector3dView vvecView;   ///< 楕円の V 軸ベクトル．
};

	}
}
